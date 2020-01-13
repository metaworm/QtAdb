#pragma once

#include <QDialog>
#include "ui_PsDlg.h"

#include "AdbDevice.h"

class PsDlg : public QDialog
{
    Q_OBJECT

public:
    PsDlg(QWidget *parent, AdbDevice *cd, int pid);
    ~PsDlg();

    void updateMemory()
    {
        int i = 0;
        for (auto l : cd->shell({ "cat", "/proc/" + QString::number(pid) + "/maps" }, true))
        {
            LineParser p(std::move(l));
            auto addr = p.next().split("-");
            auto& begin = addr[0];
            auto& end = addr[1];
            auto flag = p.next();
            auto size = p.next();
            auto f1 = p.next();
            auto f2 = p.next();
            auto image = p.next();

            ui.tableMemory->insertRow(i);
            ui.tableMemory->setItem(i, 0, new QTableWidgetItem(begin));
            ui.tableMemory->setItem(i, 1, new QTableWidgetItem(end));
            ui.tableMemory->setItem(i, 2, new QTableWidgetItem(flag));
            ui.tableMemory->setItem(i, 3, new QTableWidgetItem(size));
            ui.tableMemory->setItem(i, 4, new QTableWidgetItem(f1));
            ui.tableMemory->setItem(i, 5, new QTableWidgetItem(f2));
            ui.tableMemory->setItem(i, 6, new QTableWidgetItem(image));

            ++i;
        }
    }

private:
    Ui::PsDlg ui;

    AdbDevice *cd = nullptr;
    int pid;
};
