
#include "QtAdb.h"

QtAdb::QtAdb(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

    // 替换ComboBox组件
    comboDevice = new DeviceComboBox(this);
    ui.layout1->replaceWidget(ui.comboDevice, comboDevice);
    delete ui.comboDevice;

    // 应用列表
    ui.appList->setColumnWidth(0, 350);
    ui.appList->addAction(ui.actionStart);
    ui.appList->addAction(ui.actionStop);
    ui.appList->addAction(ui.actionUninstall);
    ui.appList->addAction(ui.actionClearSelect);
    ui.appList->addAction(ui.actionUpdateAppList);
    ui.appList->addAction(ui.actionDumpService);
    ui.appList->addAction(ui.actionDumpPackage);

    // 文件列表
    ui.tableFs->setColumnWidth(0, 280);
    ui.tableFs->setColumnWidth(1, 90);
    ui.tableFs->setColumnWidth(2, 80);
    ui.tableFs->setColumnWidth(3, 80);
    ui.tableFs->setColumnWidth(4, 80);

    // 文件树
    ui.treePs->setColumnWidth(0, 350);
    ui.treePs->setColumnWidth(1, 80);

    // 图标设置
    auto style = QApplication::style();
    setWindowIcon(style->standardIcon(QStyle::SP_TitleBarMenuButton));
    ui.actionStart->setIcon(style->standardIcon(QStyle::SP_MediaPlay));
    ui.actionStop->setIcon(style->standardIcon(QStyle::SP_MediaStop));
    ui.actionClearSelect->setIcon(style->standardIcon(QStyle::SP_DialogResetButton));
    ui.actionUninstall->setIcon(style->standardIcon(QStyle::SP_MessageBoxCritical));
    ui.actionUpdateAppList->setIcon(style->standardIcon(QStyle::SP_BrowserReload));

    // 分割比例
    ui.splitter_v->setStretchFactor(0, 2);
    ui.splitter_v->setStretchFactor(1, 1);
    ui.splitter_h->setStretchFactor(0, 1);
    ui.splitter_h->setStretchFactor(1, 2);
    ui.splitterFs->setStretchFactor(0, 1);
    ui.splitterFs->setStretchFactor(1, 4);

    connect(comboDevice, &DeviceComboBox::deviceChanged, this, &QtAdb::changeDevice);

    // 添加表格的过滤功能
    TableFilter::install(ui.appList);
    TableFilter::install(ui.treePs);
    TableFilter::install(ui.tableFs);

    // 功能数据初始化
    comboDevice->updateDevices();
}

QStringList QtAdb::getPath(QTreeWidgetItem *item)
{
    QStringList path;
    for (auto p = item; p; p = p->parent())
        path.push_front(p->text(0));
    return path;
}
