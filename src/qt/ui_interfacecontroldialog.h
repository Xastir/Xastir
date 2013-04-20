/********************************************************************************
** Form generated from reading UI file 'interfacecontroldialog.ui'
**
** Created
**      by: Qt User Interface Compiler version 4.8.4
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_INTERFACECONTROLDIALOG_H
#define UI_INTERFACECONTROLDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QDialog>
#include <QtGui/QGridLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QListWidget>
#include <QtGui/QPushButton>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_InterfaceControlDialog
{
public:
    QWidget *gridLayoutWidget;
    QGridLayout *gridLayout;
    QPushButton *startButton;
    QPushButton *stopAllButton;
    QPushButton *stopButton;
    QPushButton *addButton;
    QPushButton *startAllButton;
    QPushButton *propertiesButton;
    QPushButton *deleteButton;
    QPushButton *closeButton;
    QListWidget *listWidget;

    void setupUi(QDialog *InterfaceControlDialog)
    {
        if (InterfaceControlDialog->objectName().isEmpty())
            InterfaceControlDialog->setObjectName(QString::fromUtf8("InterfaceControlDialog"));
        InterfaceControlDialog->resize(511, 357);
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(InterfaceControlDialog->sizePolicy().hasHeightForWidth());
        InterfaceControlDialog->setSizePolicy(sizePolicy);
        gridLayoutWidget = new QWidget(InterfaceControlDialog);
        gridLayoutWidget->setObjectName(QString::fromUtf8("gridLayoutWidget"));
        gridLayoutWidget->setGeometry(QRect(20, 270, 469, 81));
        gridLayout = new QGridLayout(gridLayoutWidget);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        gridLayout->setSizeConstraint(QLayout::SetDefaultConstraint);
        gridLayout->setContentsMargins(0, 0, 0, 0);
        startButton = new QPushButton(gridLayoutWidget);
        startButton->setObjectName(QString::fromUtf8("startButton"));
        startButton->setEnabled(false);
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(startButton->sizePolicy().hasHeightForWidth());
        startButton->setSizePolicy(sizePolicy1);

        gridLayout->addWidget(startButton, 0, 0, 1, 1);

        stopAllButton = new QPushButton(gridLayoutWidget);
        stopAllButton->setObjectName(QString::fromUtf8("stopAllButton"));

        gridLayout->addWidget(stopAllButton, 1, 1, 1, 1);

        stopButton = new QPushButton(gridLayoutWidget);
        stopButton->setObjectName(QString::fromUtf8("stopButton"));
        stopButton->setEnabled(false);

        gridLayout->addWidget(stopButton, 1, 0, 1, 1);

        addButton = new QPushButton(gridLayoutWidget);
        addButton->setObjectName(QString::fromUtf8("addButton"));
        sizePolicy1.setHeightForWidth(addButton->sizePolicy().hasHeightForWidth());
        addButton->setSizePolicy(sizePolicy1);

        gridLayout->addWidget(addButton, 0, 2, 1, 1);

        startAllButton = new QPushButton(gridLayoutWidget);
        startAllButton->setObjectName(QString::fromUtf8("startAllButton"));
        sizePolicy1.setHeightForWidth(startAllButton->sizePolicy().hasHeightForWidth());
        startAllButton->setSizePolicy(sizePolicy1);

        gridLayout->addWidget(startAllButton, 0, 1, 1, 1);

        propertiesButton = new QPushButton(gridLayoutWidget);
        propertiesButton->setObjectName(QString::fromUtf8("propertiesButton"));
        propertiesButton->setEnabled(false);

        gridLayout->addWidget(propertiesButton, 1, 2, 1, 1);

        deleteButton = new QPushButton(gridLayoutWidget);
        deleteButton->setObjectName(QString::fromUtf8("deleteButton"));
        deleteButton->setEnabled(false);
        sizePolicy1.setHeightForWidth(deleteButton->sizePolicy().hasHeightForWidth());
        deleteButton->setSizePolicy(sizePolicy1);

        gridLayout->addWidget(deleteButton, 0, 3, 1, 1);

        closeButton = new QPushButton(gridLayoutWidget);
        closeButton->setObjectName(QString::fromUtf8("closeButton"));

        gridLayout->addWidget(closeButton, 1, 3, 1, 1);

        listWidget = new QListWidget(InterfaceControlDialog);
        listWidget->setObjectName(QString::fromUtf8("listWidget"));
        listWidget->setGeometry(QRect(20, 20, 471, 231));

        retranslateUi(InterfaceControlDialog);
        QObject::connect(closeButton, SIGNAL(clicked()), InterfaceControlDialog, SLOT(close()));
        QObject::connect(addButton, SIGNAL(clicked()), InterfaceControlDialog, SLOT(addInterfaceAction()));

        QMetaObject::connectSlotsByName(InterfaceControlDialog);
    } // setupUi

    void retranslateUi(QDialog *InterfaceControlDialog)
    {
        InterfaceControlDialog->setWindowTitle(QApplication::translate("InterfaceControlDialog", "Interface Control", 0, QApplication::UnicodeUTF8));
        startButton->setText(QApplication::translate("InterfaceControlDialog", "Start", 0, QApplication::UnicodeUTF8));
        stopAllButton->setText(QApplication::translate("InterfaceControlDialog", "Stop All", 0, QApplication::UnicodeUTF8));
        stopButton->setText(QApplication::translate("InterfaceControlDialog", "Stop", 0, QApplication::UnicodeUTF8));
        addButton->setText(QApplication::translate("InterfaceControlDialog", "Add", 0, QApplication::UnicodeUTF8));
        startAllButton->setText(QApplication::translate("InterfaceControlDialog", "Start All", 0, QApplication::UnicodeUTF8));
        propertiesButton->setText(QApplication::translate("InterfaceControlDialog", "Properties", 0, QApplication::UnicodeUTF8));
        deleteButton->setText(QApplication::translate("InterfaceControlDialog", "Delete", 0, QApplication::UnicodeUTF8));
        closeButton->setText(QApplication::translate("InterfaceControlDialog", "Close", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class InterfaceControlDialog: public Ui_InterfaceControlDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_INTERFACECONTROLDIALOG_H
