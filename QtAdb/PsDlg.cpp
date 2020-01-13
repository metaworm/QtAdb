#include "PsDlg.h"
#include "QtAdb.h"

PsDlg::PsDlg(QWidget *parent, AdbDevice *cd, int pid)
    : QDialog(parent), cd(cd), pid(pid)
{
    ui.setupUi(this);

    updateMemory();
    TableFilter::install(ui.tableMemory);
}

PsDlg::~PsDlg()
{
}
