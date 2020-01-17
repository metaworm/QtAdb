#pragma once

#include <QtWidgets/QMainWindow>
#include <QStandardItemModel>
#include <QHash>
#include <QSet>
#include <QComboBox>
#include <QSystemTrayIcon>
#include "ui_QtAdb.h"

#include "AdbDevice.h"
#include "PsDlg.h"

using namespace std;

class DeviceComboBox : public QComboBox
{
    Q_OBJECT

public:
    DeviceComboBox(QWidget *p): QComboBox(p)
    {
        connect(this,
            static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &DeviceComboBox::onChanged);
    }

    void onChanged(int i)
    {
        emit deviceChanged((AdbDevice*)itemData(i).value<quintptr>());
    }

    // 更新设备列表
    void updateDevices()
    {
        auto oldDevs = devs;
        auto set = QSet<QString>::fromList(AdbDevice::devices());
        // 更新已有列表
        for (int i = 0; i < count(); ++i)
        {
            auto devName = itemText(i).split(QRegExp("\\s+"))[0];
            auto dev = (AdbDevice*)itemData(i).value<quintptr>();
            if (set.remove(devName))
            {
                // 手机型号
                setItemText(i, devName + "\t" + dev->model());
            }
            else
            {
                // 列表里不存在的删掉
                removeItem(i);
                delete dev;
            }
        }
        // 添加新的设备
        for (auto devName : set)
        {
            auto dev = new AdbDevice(devName);
            // 手机型号
            addItem(devName + "\t" + dev->model(), (quintptr)dev);
        }
    }

protected:
    void showPopup()
    {
        updateDevices();
        QComboBox::showPopup();
    }

Q_SIGNALS:
    void deviceChanged(AdbDevice*);

private:
    QHash<QString, int> devs;
};

class TableFilter: public QLineEdit
{
    Q_OBJECT

public:
    static void install(QWidget *parent)
    {
        auto t = (TableFilter*)parent->property("filter").value<quintptr>();
        if (!t) parent->setProperty("filter", (quintptr)(new TableFilter(parent)));
    }

    static void display(QObject *parent)
    {
        auto t = (TableFilter*)parent->property("filter").value<quintptr>();
        if (t) t->display();
    }

private:
    int timer = 0;

    TableFilter(QWidget *parent): QLineEdit(parent)
    {
        setPlaceholderText("过滤");
        parent->installEventFilter(this);
        this->hide();
        connect(this, &QLineEdit::textChanged, this, &TableFilter::onChanged);
        auto pl = palette();
        pl.setColor(QPalette::Background, QColor(0, 0, 0, 100));
        setPalette(pl);
    }

    void display()
    {
        const int padding = 1;
        const int height = 30;
        auto p = (QWidget*)parent();
        auto& g = p->geometry();
        setGeometry(padding, g.height() - height, g.width() - padding, height);
        //raise(); p->stackUnder(topLevelWidget());
        show(); setFocus();
    }

    void doFilter(QTreeWidget *p)
    {
        if (!p) return;
        QSet<QTreeWidgetItem*> set; // 匹配的行
        for (auto i : p->findItems(text(), Qt::MatchContains))
            set.insert(i);
        QTreeWidgetItemIterator it(p);
        while (*it)
        {
            auto i = *it++;
            i->setHidden(!set.contains(i));
        }
    }

    void doFilter(QTableWidget *p)
    {
        if (!p) return;
        QSet<int> set; // 匹配的行
        for (auto i : p->findItems(text(), Qt::MatchContains))
            set.insert(i->row());
        for (int i = 0; i < p->rowCount(); i++)
            p->setRowHidden(i, !set.contains(i));
    }

    void doFilter()
    {
        doFilter(qobject_cast<QTableWidget*>(parent()));
        doFilter(qobject_cast<QTreeWidget*>(parent()));
    }

public slots:
    void onChanged(const QString& text)
    {
        if (timer) killTimer(timer);
        timer = startTimer(500);
    }

protected:
    // 超过一定时间不按键，进行过滤
    void timerEvent(QTimerEvent *event)
    {
        killTimer(timer);
        doFilter();
    }

    void keyPressEvent(QKeyEvent *e)
    {
        if (e->key() == Qt::Key_Return)
            return doFilter();
        if (e->key() == Qt::Key_Escape)
            return hide();
        QLineEdit::keyPressEvent(e);
    }

    // 为父窗口绑定Ctrl+F快捷键
    bool eventFilter(QObject *target, QEvent *event)
    {
        if (target == parent() && event->type() == QEvent::KeyPress)
        {
            QKeyEvent *e = static_cast<QKeyEvent*>(event);
            if (e->key() == Qt::Key_F && e->modifiers() == Qt::ControlModifier)
            {
                display(target);
                return true;
            }
        }
        return target->eventFilter(target, event);
    }
};

class QtAdb : public QMainWindow
{
	Q_OBJECT

public:
	QtAdb(QWidget *parent = Q_NULLPTR);

    void log(const QString& text)
    {
        ui.logEdit->appendPlainText(text);
    }

    //void log(QString text) { ui.logEdit->appendPlainText(text); }

    void log(const QStringList& list)
    {
        for (auto& s : list) log(s);
    }

    // 打印详细信息
    void logBasicInfo()
    {
        if (!checkDevice()) return;

        log("[型号]");
        log(cd->shell({ "getprop", "ro.product.model" }));
        log("[安卓版本]");
        log(cd->shell({ "getprop", "ro.build.version.release" }) + cd->shell({ "getprop", "ro.product.name" }));
        log("[分辨率]");
        log(cd->shell({ "wm", "size" }));
    }

    QString storageSize(float size)
    {
        const char *units[] = { "KB", "MB", "GB", "TB", nullptr };
        int i = -1;
        while (size > 1024) size /= 1024, ++i;
        auto result = QString::number(size, 'g', 2);
        if (i >= 0) result.append(' ').append(units[i]);
        return result;
    }

    void updatePsTree()
    {
        if (!checkDevice()) return;

        bool first = ui.treePs->topLevelItemCount() == 0;
        for (auto line : cd->shell({ "ps", "-A", "-o", "NAME,PID,PPID,USER,VSZ,CMDLINE", "|", "tail", "-n", "+2"}))
        {
            LineParser p(std::move(line));
            auto name = p.psname();
            // 忽略线程
            if (name[0] == '[') continue;

            auto pid = p.next();
            auto ppid = p.next().toInt();
            auto user = p.next();
            auto mem = p.next();
            auto path = p.rest();

            auto pid_ = pid.toInt();
            auto item = psMap[pid_];
            if (!item) item = new QTreeWidgetItem();

            item->setText(0, name);
            item->setText(1, pid);
            item->setText(2, user);
            item->setText(3, storageSize(mem.toFloat()));
            item->setText(4, path);

            auto parent = psMap[ppid];
            if (parent)
                parent->addChild(item);
            else if (first)
                ui.treePs->addTopLevelItem(item);
            psMap.insert(pid_, item);
        }

        if (first) ui.treePs->expandAll();
    }

public slots:
    void changeDevice(AdbDevice *dev)
    {
        cd = dev; 
        logBasicInfo();
        onTabChanged(ui.tabWidget->currentIndex());
    }

    bool checkDevice()
    {
        if (cd) return true;
        QSystemTrayIcon tray(this);
        tray.showMessage("", "没有设备");
        return false;
    }

    void updateAppList()
    {
        if (!checkDevice()) return;

        auto data = cd->shell({ "pm", "list", "package", "-f" }).split();
        QRegExp reg(R"(package:(.+)=([^=]+)$)");
        ui.appList->setRowCount(data.size());
        for (int i = 0; i < data.size(); ++i)
        {
            if (data[i].indexOf(reg) < 0)
            {
                ui.appList->removeRow(i);
                log("[warn] " + data[i]);
                continue;
            }

            auto path = reg.cap(1);
            auto package = reg.cap(2);
            ui.appList->setItem(i, 0, new QTableWidgetItem(package));
            ui.appList->setItem(i, 1, new QTableWidgetItem(path));
        }
    }

    void onTabChanged(int i)
    {
        auto label = ui.tabWidget->tabText(i);
        if (label == "APP")
            updateAppList();
        if (label == "进程")
            updatePsTree();
        if (label == "文件")
        {
            if (!ui.treeFs->topLevelItemCount()) updateDirs("/");
        }
    }
    
    void uninstall()
    {
        // 当前焦点
        auto app = ui.appList->item(ui.appList->currentRow(), 0)->text();
        if (QMessageBox::information(this, "卸载应用", app, QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            auto r = cd->shell({ "pm", "uninstall", "--user", "0", app });
            log("Uninstall " + app + " :" + r);
            updateAppList();
        }
    }

    void clearAppSelection()
    {
        ui.appList->clearSelection();
    }

    void startApp()
    {
        if (!checkDevice()) return;

        auto app = ui.appList->item(ui.appList->currentRow(), 0)->text();
        auto act = cd->shell({ "dumpsys package " + app + " | awk '/android.intent.action.MAIN:/ { getline; print $2 }'" }).split();
        if (act.size()) log(cd->shell({ "am", "start", act[0] }));
    }

    // 输入文本
    void inputText()
    {
        if (!checkDevice()) return;
        cd->shell({ "input", "text", ui.lineInputText->text() });
    }

    void execActionCommand()
    {
        if (!checkDevice()) return;
        auto app = ui.appList->item(ui.appList->currentRow(), 0)->text();
        auto a = (QAction*)sender();
        log("[" + a->text() + ": " + app + "]");
        log(cd->shell({ a->toolTip(), app}));
    }

    void execShellCommand(QString cmd)
    {
        if (!checkDevice()) return;

        log("$ " + cmd);
        log(cd->shell({ cmd }));
    }

    void onCommandDblClicked(QTableWidgetItem *item)
    {
        execShellCommand(item->text());
    }

    ShellResult ls(const QString& dir)
    {
        return cd->shell({ "ls -lh " + dir + " | awk 'NR>1{sub(/, +/,\",\");print}'" });
    }

    static QStringList getPath(QTreeWidgetItem *item);

    void onFileExpanded(QTreeWidgetItem *item)
    {
        QStringList path = getPath(item);
        path.front() = "";
        path.push_back("");

        if (0 == item->childCount())
        {
            updateDirs(path.join("/"), item);
        }
    }

    void onFileItemChanged(QTreeWidgetItem *item, QTreeWidgetItem *old)
    {
        onFileExpanded(item);
    }

    void updateDirs(const QString& dir, QTreeWidgetItem *parent = nullptr)
    {
        if (!checkDevice()) return;

        auto style = QApplication::style();
        if (!parent)
        {
            parent = new QTreeWidgetItem();
            parent->setText(0, "/");
            parent->setIcon(0, style->standardIcon(QStyle::SP_DirIcon));
            parent->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
            ui.treeFs->addTopLevelItem(parent);
            return updateDirs("/", parent);
        }

        QRegExp deli("\\s+");
        auto data = ls(dir).split();
        ui.tableFs->setRowCount(data.size());
        for (int i = 0; data.size(); ++i)
        {
            LineParser p(std::move(data.takeFirst()));

            auto flags = p.next();
            p.next();
            auto user = p.next();
            auto group = p.next();
            auto size = p.next();
            p.next();
            p.next();
            auto name = p.next();
            p.next();
            auto link = p.next();

            auto file = new QTableWidgetItem(name);
            ui.tableFs->setItem(i, 0, file);
            ui.tableFs->setItem(i, 1, new QTableWidgetItem(flags));
            ui.tableFs->setItem(i, 2, new QTableWidgetItem(user));
            ui.tableFs->setItem(i, 3, new QTableWidgetItem(group));
            ui.tableFs->setItem(i, 4, new QTableWidgetItem(size));

            auto item = new QTreeWidgetItem();
            item->setText(0, name);

            bool isDir = false;
            bool isLink = false;
            if (flags[0] == 'l' && link.size())
                isLink = true, isDir = size == "11";
            else if (flags[0] == 'd') isDir = true;

            ui.tableFs->setItem(i, 4, new QTableWidgetItem(size));
            ui.tableFs->setItem(i, 5, new QTableWidgetItem(isLink ? link: ""));
            if (isDir)
            {
                if (parent) parent->addChild(item);
                auto icon = style->standardIcon(isLink ? QStyle::SP_DirLinkIcon : QStyle::SP_DirIcon);
                file->setIcon(icon), item->setIcon(0, icon);
                item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
            }
            else
            {
                auto icon = style->standardIcon(isLink ? QStyle::SP_FileLinkIcon : QStyle::SP_FileIcon);
                file->setIcon(icon), item->setIcon(0, icon);
            }
        }
        parent->setExpanded(true);
    }

    void onPsTreeItemDoubleClicked(QTreeWidgetItem *item)
    {
        auto dlg = new PsDlg(this, cd, item->text(1).toInt());
        dlg->setModal(true);
        dlg->show();
    }

private:
	Ui::QtAdbClass ui;

    DeviceComboBox *comboDevice;
    QHash<int, QTreeWidgetItem*> psMap;
    AdbDevice *cd = nullptr;
    QActionGroup *devGroup = new QActionGroup(this);
};
