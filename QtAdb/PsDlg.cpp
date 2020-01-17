#include "PsDlg.h"
#include "QtAdb.h"

PsDlg::PsDlg(QWidget *parent, AdbDevice *cd, int pid)
    : QDialog(parent), cd(cd), pid(pid)
{
    ui.setupUi(this);

    onTabChanged(ui.tabWidget->currentIndex());
    TableFilter::install(ui.tableMemory);
}

PsDlg::~PsDlg()
{
}
