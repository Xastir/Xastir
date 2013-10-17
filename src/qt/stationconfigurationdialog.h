#ifndef STATIONCONFIGURATIONDIALOG_H
#define STATIONCONFIGURATIONDIALOG_H

#include <QDialog>
#include "stationsettings.h"

namespace Ui {
class StationConfigurationDialog;
}

class StationConfigurationDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit StationConfigurationDialog(StationSettings *settings, QWidget *parent = 0);
    ~StationConfigurationDialog();

private slots:
    void disablePHBChanged(int state);
    void symbolSettinngsChanged(QString t);
    void accept();

private:
    Ui::StationConfigurationDialog *ui;
    StationSettings *mySettings;
};

#endif // STATIONCONFIGURATIONDIALOG_H
