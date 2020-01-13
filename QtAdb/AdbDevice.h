#ifndef __ADBDEVICE_H__
#define __ADBDEVICE_H__

#include <QTextCodec>
#include <QSettings>
#include <QString>
#include <QDebug>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegExp>
#include <QJsonDocument>
#include <QJsonObject>
#include <QXmlStreamReader>

#include <Windows.h>
#include <shellscalingapi.h>

#include <chrono>
#include <vector>

#pragma comment(lib, "Shcore.lib")

using namespace std;
using namespace chrono;

struct Region
{
	float x1, x2;
	float y1, y2;

	Region(): x1(0.0), x2(1.0), y1(0.0), y2(1.0) {}

	Region& top(float n) { y2 = n; return *this; }
	Region& bot(float n) { y1 = 1.0 - n; return *this; }
	Region& left(float n) { x1 = n; return *this; }
	Region& right(float n) { x2 = 1.0 - n; return *this; }
};

// 控件定位器
struct CtrlLocator
{
	QString text;
	QString cls;
	QString rc;

	CtrlLocator(const QString& name, const QString& ty = "", const QString& rc = "")
		: text(name), cls(ty), rc(rc) {}

	CtrlLocator(const char *name, const char *ty = "", const char *rc = "")
		: text(name), cls(ty), rc(rc) {}

	static CtrlLocator by_rc(const QString& name) { return CtrlLocator("", "", name); }
};

struct ShellResult: public QByteArray
{
    ShellResult(QByteArray&& data): QByteArray(data) {}

    // 所有输出转换成字符串
    inline operator QString()
    {
        return QString::fromUtf8(*this);
    }

    struct iter
    {
        const QByteArray *arr;
        int next_b = 0;
        int next_e;

        iter(const ShellResult *a, int size) : arr(a)
        {
            next_b = next_e = size;
        }

        iter(const ShellResult *a): arr(a)
        {
            next_e = arr->indexOf("\r\n", next_b);
        }

        iter& operator++()
        {
            next_b = next_e + 2;
            next_e = arr->indexOf("\r\n", next_b);
            return *this;
        }

        //后缀自加
        iter operator++(int)
        {
            auto r = *this;
            ++*this;
            return r;
        }

        QString operator*() const
        {
            return QString::fromUtf8(arr->data() + next_b, next_e - next_b);
        }

        bool operator==(const iter &arg) const
        {
            return next_b == arg.next_b || next_e < 0;
        }

        bool operator!=(const iter &arg) const
        {
            return !(arg == *this);
        }
    };

    iter begin() const { return iter(this); }
    iter end() const { return iter(this, this->size()); }

    QStringList split()
    {
        auto l = ((QString)(*this)).split("\r\n");
		if (l.size() > 0 && l.last().size() == 0)
			l.pop_back();		// 去除最后的空行
        return l;
    }
};

struct LineParser: public QString
{
    LineParser(QString&& str): QString(str) {}

    // 一串连续的非空格字符串
    QString next()
    {
        skip_white();
        auto pos = cur_pos;
        auto& p = cur_pos;
        while (p < size() && !at(p).isSpace()) ++p;
        return mid(pos, p - pos);
    }

    QString readTo(QChar c)
    {
        skip_white();
        auto pos = cur_pos;
        auto& p = cur_pos;
        while (p < size() && at(p) != c) ++p;
        return mid(pos, ++p - pos);
    }

    QString psname()
    {
        skip_white();
        return at(cur_pos) == '[' ? readTo(']') : next();
    }

    QString rest(bool skip = true)
    {
        if (skip) skip_white();
        return mid(cur_pos);
    }

    void skip_white()
    {
        while (cur_pos < size() && at(cur_pos).isSpace()) ++cur_pos;
    }

private:
    int cur_pos = 0;
};

// 模拟器
class AdbDevice
{
public:
	// 执行adb命令，并返回输出结果(二进制)
	static ShellResult adb(const QStringList& args)
	{
        QProcess p;
        p.start("adb.exe", args);
		p.waitForFinished();
		return p.readAll();
	}

    static QStringList adb_l(const QStringList& args)
    {
		return adb(args).split();
    }

    QStringList adb_shell(const QStringList& args)
    {
        return adb_l(QStringList{"-s", name, "shell"} + args);
    }

    ShellResult shell(const QStringList& args, bool root = false)
    {
        return root ? adb(QStringList{"-s", name, "shell", "su", "-c"} + args)
                    : adb(QStringList{"-s", name, "shell"} + args);
    }

    // 设备型号
    QString model()
    {
        return shell({ "getprop", "ro.product.model" }).trimmed();
    }

    // 设备列表
    static QList<QString> devices()
    {
        auto l = adb_l(QStringList{ "devices" });
        QList<QString> result;
        for (int i = 1; i < l.size(); ++i)
        {
            auto sl = l[i].split(QRegExp("\\s+"));
            if (sl.size() > 1) result.push_back(sl[0]);
        }
        return result;
    }

    AdbDevice(const QString& name): name(name) {}

	// 输入文字
	void input(const QString& text)
	{
		//LdConsole::ldconsole(QStringList{
		//	"action", "--index", index(),
		//	"--key", "call.input", "--value", text
		//});
	}

	struct EmuPoint
	{
		int x, y;
	};

	// 点击屏幕
	void tap(const EmuPoint& p)
	{
        adb_shell(QStringList{
            "input", "tap",
            QString::number(p.x), QString::number(p.y),
        });
	}

	// 点击屏幕
	inline void tap(int x, int y) { tap(EmuPoint { x, y }); }

	// 控件边界
	struct CtrlBound
	{
		int left, top, right, bottom;

		CtrlBound(): left(0), top(0), right(0), bottom(0) {}

		operator bool() { return right > left && bottom > top; }

		EmuPoint center() const { return EmuPoint {(right + left) / 2, (bottom + top) / 2}; }
	};

	// 点击屏幕
	inline void tap(const CtrlBound& b) { tap(b.center()); }

	// 寻找控件位置
	CtrlBound find_ctrl(const CtrlLocator& l)
	{
		CtrlBound bound;

  //      QRegExp reg(R"(to:\s*(\S+)$)");
		//auto s = adb_shell(QStringList {"uiautomator", "dump", "--compressed"});
  //      if (!(s.size() && s[0].indexOf(reg) > 0))
  //          return bound;

  //      auto xml_path = reg.cap(1);
  //      qDebug() << "xml path: " << xml_path;

        auto xml_path = "/sdcard/window_dump.xml";

		adb_shell(QStringList {"uiautomator", "dump", "--compressed"});
		auto data = adb_shell(QStringList {"cat", xml_path, ";", "rm", xml_path});
        if (data.isEmpty()) return bound;

		QString result;
		QXmlStreamReader xml(data[0]);
		while (!xml.atEnd() && !xml.hasError())
		{
			//读取下一个element.
			QXmlStreamReader::TokenType token = xml.readNext();
			//如果获取了StartElement,则尝试读取
			if (token == QXmlStreamReader::StartElement && xml.name() == "node")
			{
				QXmlStreamAttributes attr = xml.attributes();

				// ResouceID 单独定位
				if (l.rc.size() > 0)
				{
					if (attr.value("resource-id").toString() == l.rc)
					{
						result = attr.value("bounds").toString(); break;
					}
					else continue;
				}

				// 结合class和text定位
				if (l.cls.size() > 0)
				{
					if (attr.value("class").toString() != l.cls) continue;
				}
				if (attr.value("text").toString() == l.text)
				{
					result = attr.value("bounds").toString(); break;
				}
			}
		}

		if (result.size() > 0)
		{
			QRegExp reg(R"(\[(\d+),(\d+)\]\[(\d+),(\d+)\])");
			auto pos = result.indexOf(reg);	// TODO: assert(pos >= 0)
			bound.left = reg.cap(1).toInt();
			bound.top = reg.cap(2).toInt();
			bound.right = reg.cap(3).toInt();
			bound.bottom = reg.cap(4).toInt();
		}
		return bound;
	}

	// 等待某个控件出现在当前页面
	CtrlBound wait_ctrl(const CtrlLocator& l, int ms = 100 * 1000)
	{
		auto t = system_clock::now();
		CtrlBound bound;
		do {
			if (bound = find_ctrl(l)) break;
		} while (duration_cast<milliseconds>(system_clock::now() - t).count() < ms);
		return bound;
	}

	// 寻找控件位置
	CtrlBound click_ctrl(const QString& text, int ms = 100 * 1000)
	{
		auto t = system_clock::now();
		auto bound = wait_ctrl(text, ms);
		if (bound) { tap(bound.center()); }
		return bound;
	}

	// 寻找控件位置
	CtrlBound click_rc(const QString& rc, int ms = 100 * 1000)
	{
		auto bound = wait_ctrl(CtrlLocator::by_rc(rc), ms);
		if (bound) tap(bound.center());
		return bound;
	}

	// 启动应用
	// com.ss.android.ugc.live/com.ss.android.ugc.live.main.MainActivity
	void start_app(const QString& pkg_act)
	{
		adb_shell(QStringList {"am", "start", "-n", pkg_act});
	}

	// 获取当前Activity
	QString activity(QString* pkg = nullptr)
	{
        QString result;
		auto out = adb_shell(QStringList{"dumpsys window w | grep name="});
        for (int i = 0; i < out.size() - 1; ++i)
            if (out[i].contains("name=StatusBar"))
            {
                QRegExp reg(R"(name=([\w\.]+)\/([\w\.]+))");
                auto w = out[i + 1];
                if (w.indexOf(reg) > 0)
                {
                    if (pkg) *pkg = reg.cap(1);
                    result = reg.cap(2);
                    break;
                }
            }
		return result;
	}

	bool wait_activity(const QString& name, int timeout = 100 * 1000)
	{
		auto t = system_clock::now();
		do {
			if (activity() == name) return true;
			Sleep(100);
		} while (duration_cast<milliseconds>(system_clock::now() - t).count() < timeout);
		return false;
	}

	// 某个APP是否处于活动状态
	bool app_active(const QString& name)
	{
		return adb_shell(QStringList {"dumpsys", "window", "|", "grep", name}).size() > 0;
	}

	QString name;		// 设备名称
};

#endif // __ADBDEVICE_H__
