#include "QtAdb.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	QtAdb w;
	w.show();
	return a.exec();
}
