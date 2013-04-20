/********************************************************************************
** Form generated from reading UI file 'netinterfacepropertiesdialog.ui'
**
** Created
**      by: Qt User Interface Compiler version 4.8.4
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_NETINTERFACEPROPERTIESDIALOG_H
#define UI_NETINTERFACEPROPERTIESDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QDialog>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_NetInterfacePropertiesDialog
{
public:
    QDialogButtonBox *buttonBox;
    QWidget *layoutWidget;
    QVBoxLayout *verticalLayout;
    QCheckBox *activateOnStartup;
    QCheckBox *allowTransmitting;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QLineEdit *hostEdit;
    QHBoxLayout *horizontalLayout_5;
    QLabel *label_2;
    QLineEdit *portEdit;
    QLabel *label_7;
    QLineEdit *callsignEdit;
    QHBoxLayout *horizontalLayout_2;
    QLabel *label_3;
    QLineEdit *passcodeEdit;
    QLabel *label_4;
    QHBoxLayout *horizontalLayout_3;
    QLabel *label_5;
    QLineEdit *filterEdit;
    QHBoxLayout *horizontalLayout_4;
    QLabel *label_6;
    QLineEdit *commentEdit;
    QCheckBox *reconnectOnFailure;

    void setupUi(QDialog *NetInterfacePropertiesDialog)
    {
        if (NetInterfacePropertiesDialog->objectName().isEmpty())
            NetInterfacePropertiesDialog->setObjectName(QString::fromUtf8("NetInterfacePropertiesDialog"));
        NetInterfacePropertiesDialog->resize(450, 347);
        buttonBox = new QDialogButtonBox(NetInterfacePropertiesDialog);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        buttonBox->setGeometry(QRect(30, 300, 391, 32));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
        layoutWidget = new QWidget(NetInterfacePropertiesDialog);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        layoutWidget->setGeometry(QRect(30, 20, 391, 268));
        verticalLayout = new QVBoxLayout(layoutWidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        activateOnStartup = new QCheckBox(layoutWidget);
        activateOnStartup->setObjectName(QString::fromUtf8("activateOnStartup"));

        verticalLayout->addWidget(activateOnStartup);

        allowTransmitting = new QCheckBox(layoutWidget);
        allowTransmitting->setObjectName(QString::fromUtf8("allowTransmitting"));

        verticalLayout->addWidget(allowTransmitting);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        label = new QLabel(layoutWidget);
        label->setObjectName(QString::fromUtf8("label"));

        horizontalLayout->addWidget(label);

        hostEdit = new QLineEdit(layoutWidget);
        hostEdit->setObjectName(QString::fromUtf8("hostEdit"));

        horizontalLayout->addWidget(hostEdit);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setObjectName(QString::fromUtf8("horizontalLayout_5"));
        label_2 = new QLabel(layoutWidget);
        label_2->setObjectName(QString::fromUtf8("label_2"));

        horizontalLayout_5->addWidget(label_2);

        portEdit = new QLineEdit(layoutWidget);
        portEdit->setObjectName(QString::fromUtf8("portEdit"));
        QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(portEdit->sizePolicy().hasHeightForWidth());
        portEdit->setSizePolicy(sizePolicy);

        horizontalLayout_5->addWidget(portEdit);

        label_7 = new QLabel(layoutWidget);
        label_7->setObjectName(QString::fromUtf8("label_7"));

        horizontalLayout_5->addWidget(label_7);

        callsignEdit = new QLineEdit(layoutWidget);
        callsignEdit->setObjectName(QString::fromUtf8("callsignEdit"));

        horizontalLayout_5->addWidget(callsignEdit);


        verticalLayout->addLayout(horizontalLayout_5);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        label_3 = new QLabel(layoutWidget);
        label_3->setObjectName(QString::fromUtf8("label_3"));

        horizontalLayout_2->addWidget(label_3);

        passcodeEdit = new QLineEdit(layoutWidget);
        passcodeEdit->setObjectName(QString::fromUtf8("passcodeEdit"));

        horizontalLayout_2->addWidget(passcodeEdit);

        label_4 = new QLabel(layoutWidget);
        label_4->setObjectName(QString::fromUtf8("label_4"));

        horizontalLayout_2->addWidget(label_4);


        verticalLayout->addLayout(horizontalLayout_2);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        label_5 = new QLabel(layoutWidget);
        label_5->setObjectName(QString::fromUtf8("label_5"));

        horizontalLayout_3->addWidget(label_5);

        filterEdit = new QLineEdit(layoutWidget);
        filterEdit->setObjectName(QString::fromUtf8("filterEdit"));

        horizontalLayout_3->addWidget(filterEdit);


        verticalLayout->addLayout(horizontalLayout_3);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setObjectName(QString::fromUtf8("horizontalLayout_4"));
        label_6 = new QLabel(layoutWidget);
        label_6->setObjectName(QString::fromUtf8("label_6"));

        horizontalLayout_4->addWidget(label_6);

        commentEdit = new QLineEdit(layoutWidget);
        commentEdit->setObjectName(QString::fromUtf8("commentEdit"));

        horizontalLayout_4->addWidget(commentEdit);


        verticalLayout->addLayout(horizontalLayout_4);

        reconnectOnFailure = new QCheckBox(layoutWidget);
        reconnectOnFailure->setObjectName(QString::fromUtf8("reconnectOnFailure"));

        verticalLayout->addWidget(reconnectOnFailure);


        retranslateUi(NetInterfacePropertiesDialog);
        QObject::connect(buttonBox, SIGNAL(accepted()), NetInterfacePropertiesDialog, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), NetInterfacePropertiesDialog, SLOT(reject()));

        QMetaObject::connectSlotsByName(NetInterfacePropertiesDialog);
    } // setupUi

    void retranslateUi(QDialog *NetInterfacePropertiesDialog)
    {
        NetInterfacePropertiesDialog->setWindowTitle(QApplication::translate("NetInterfacePropertiesDialog", "Configure Internet", 0, QApplication::UnicodeUTF8));
        activateOnStartup->setText(QApplication::translate("NetInterfacePropertiesDialog", "Activate on Startup?", 0, QApplication::UnicodeUTF8));
        allowTransmitting->setText(QApplication::translate("NetInterfacePropertiesDialog", "Allow Transmitting?", 0, QApplication::UnicodeUTF8));
        label->setText(QApplication::translate("NetInterfacePropertiesDialog", "Host:", 0, QApplication::UnicodeUTF8));
        label_2->setText(QApplication::translate("NetInterfacePropertiesDialog", "Port:", 0, QApplication::UnicodeUTF8));
        label_7->setText(QApplication::translate("NetInterfacePropertiesDialog", "Callsign:", 0, QApplication::UnicodeUTF8));
        label_3->setText(QApplication::translate("NetInterfacePropertiesDialog", "Pass-code:", 0, QApplication::UnicodeUTF8));
        label_4->setText(QApplication::translate("NetInterfacePropertiesDialog", "(Leave blank if none)", 0, QApplication::UnicodeUTF8));
        label_5->setText(QApplication::translate("NetInterfacePropertiesDialog", "Filter Parameters:", 0, QApplication::UnicodeUTF8));
        label_6->setText(QApplication::translate("NetInterfacePropertiesDialog", "Comment:", 0, QApplication::UnicodeUTF8));
        reconnectOnFailure->setText(QApplication::translate("NetInterfacePropertiesDialog", "Reconnect on Net Failure?", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class NetInterfacePropertiesDialog: public Ui_NetInterfacePropertiesDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_NETINTERFACEPROPERTIESDIALOG_H
