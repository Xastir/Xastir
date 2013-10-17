#include "stationconfigurationdialog.h"
#include "ui_stationconfigurationdialog.h"
#include "stationsettings.h"
#include <QtDebug>
#include <QIcon>
#include "symbols.h"

StationConfigurationDialog::StationConfigurationDialog(StationSettings *settings, QWidget *parent ) :
    QDialog(parent),
    ui(new Ui::StationConfigurationDialog),
    mySettings(settings)
{
    ui->setupUi(this);
    connect(ui->disablePHGCheckbox,SIGNAL(stateChanged(int)),this,SLOT(disablePHBChanged(int)));
    connect(ui->groupEdit, SIGNAL(textChanged(QString)), this, SLOT(symbolSettingsChanged(QString)));
    connect(ui->symbolEdit, SIGNAL(textChanged(QString)), this, SLOT(symbolSettingsChanged(QString)));


    ui->callsignEdit->setText(settings->callsign());
    ui->compressedPacketsCheckbox->setChecked(settings->sendCompressed());
    /* XXX Set lat and lon */
    disablePHBChanged(!settings->hasPHGD());
    ui->disablePHGCheckbox->setChecked(!settings->hasPHGD());
    ui->commentLineEdit->setText(settings->comment());
    ui->groupEdit->setText(QString(settings->group()));
    ui->symbolEdit->setText(QString(settings->symbol()));

    char key[4];
    key[0] = '/';
    key[1] = 'y';
    key[2] = ' ';
    key[3] = 0;
    Symbol *sym = symbol_data[key];
    QPixmap pix = sym->pix;
    ui->symbolSelectButton->setIcon(QIcon(pix));
}

StationConfigurationDialog::~StationConfigurationDialog()
{
    delete ui;
}

void StationConfigurationDialog::disablePHBChanged(int state)
{
    ui->powerCombo->setEnabled(!state);
    ui->gainCombo->setEnabled(!state);
    ui->heightCombo->setEnabled(!state);
    ui->directivityCombo->setEnabled(!state);
}

void StationConfigurationDialog::symbolSettingsChanged(QString t)
{
    Q_UNUSED(t)
    char key[4];
    key[0] = ui->groupEdit->text()[0].cell();
    key[1] = ui->symbolEdit->text()[0].cell();
    key[2] = ' ';
    key[3] = 0;

    if(symbol_data.contains(key)) {
        ui->icon->setPixmap(symbol_data[key]->pix);
    } else {
        ui->icon->clear();
    }
}

void StationConfigurationDialog::accept()
{
    mySettings->setCallsign(ui->callsignEdit->text());
    mySettings->setSendCompressed(ui->compressedPacketsCheckbox->isChecked());
    mySettings->setComment(ui->commentLineEdit->text());
    mySettings->setGroup(ui->groupEdit->text()[0].cell());
    mySettings->setSymbol(ui->symbolEdit->text()[0].cell());
    mySettings->sethasPHGD(!ui->disablePHGCheckbox->isChecked());
    QDialog::accept();
}
