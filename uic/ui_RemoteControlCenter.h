/********************************************************************************
** Form generated from reading UI file 'RemoteControlCenter.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_REMOTECONTROLCENTER_H
#define UI_REMOTECONTROLCENTER_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_RemoteControlCenter
{
public:
    QVBoxLayout *verticalLayout;
    QGroupBox *serviceGroup;
    QHBoxLayout *serviceLayout;
    QRadioButton *remoteOffRadio;
    QRadioButton *remoteOnRadio;
    QSpacerItem *serviceSpacer;
    QLabel *serviceSummaryLabel;
    QTabWidget *modeTabWidget;
    QWidget *wifiTab;
    QVBoxLayout *wifiLayout;
    QLabel *wifiHintLabel;
    QLabel *wifiEndpointTitle;
    QLabel *wifiEndpointLabel;
    QLabel *wifiTokenTitle;
    QLabel *wifiTokenLabel;
    QLabel *wifiStatusLabel;
    QPushButton *wifiCopyBtn;
    QLabel *bleDeviceTitle;
    QLabel *bleDeviceLabel;
    QLabel *bleStatusLabel;
    QSpacerItem *wifiSpacer;
    QWidget *qrTab;
    QVBoxLayout *qrLayout;
    QLabel *qrImageLabel;
    QHBoxLayout *qrIpRow;
    QLabel *qrIpTitle;
    QComboBox *qrIpCombo;
    QLabel *qrUrlLabel;
    QLabel *qrStatusLabel;
    QHBoxLayout *qrBtnRow;
    QPushButton *qrRefreshBtn;
    QPushButton *qrCopyBtn;
    QPushButton *qrDisconnectBtn;
    QHBoxLayout *bottomLayout;
    QSpacerItem *bottomSpacer;
    QPushButton *closeWindowBtn;

    void setupUi(QDialog *RemoteControlCenter)
    {
        if (RemoteControlCenter->objectName().isEmpty())
            RemoteControlCenter->setObjectName(QString::fromUtf8("RemoteControlCenter"));
        RemoteControlCenter->resize(480, 560);
        RemoteControlCenter->setMinimumSize(QSize(440, 480));
        verticalLayout = new QVBoxLayout(RemoteControlCenter);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        serviceGroup = new QGroupBox(RemoteControlCenter);
        serviceGroup->setObjectName(QString::fromUtf8("serviceGroup"));
        serviceLayout = new QHBoxLayout(serviceGroup);
        serviceLayout->setObjectName(QString::fromUtf8("serviceLayout"));
        remoteOffRadio = new QRadioButton(serviceGroup);
        remoteOffRadio->setObjectName(QString::fromUtf8("remoteOffRadio"));
        remoteOffRadio->setChecked(true);

        serviceLayout->addWidget(remoteOffRadio);

        remoteOnRadio = new QRadioButton(serviceGroup);
        remoteOnRadio->setObjectName(QString::fromUtf8("remoteOnRadio"));
        remoteOnRadio->setChecked(false);

        serviceLayout->addWidget(remoteOnRadio);

        serviceSpacer = new QSpacerItem(16, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        serviceLayout->addItem(serviceSpacer);

        serviceSummaryLabel = new QLabel(serviceGroup);
        serviceSummaryLabel->setObjectName(QString::fromUtf8("serviceSummaryLabel"));

        serviceLayout->addWidget(serviceSummaryLabel);


        verticalLayout->addWidget(serviceGroup);

        modeTabWidget = new QTabWidget(RemoteControlCenter);
        modeTabWidget->setObjectName(QString::fromUtf8("modeTabWidget"));
        wifiTab = new QWidget();
        wifiTab->setObjectName(QString::fromUtf8("wifiTab"));
        wifiLayout = new QVBoxLayout(wifiTab);
        wifiLayout->setObjectName(QString::fromUtf8("wifiLayout"));
        wifiHintLabel = new QLabel(wifiTab);
        wifiHintLabel->setObjectName(QString::fromUtf8("wifiHintLabel"));
        wifiHintLabel->setWordWrap(true);

        wifiLayout->addWidget(wifiHintLabel);

        wifiEndpointTitle = new QLabel(wifiTab);
        wifiEndpointTitle->setObjectName(QString::fromUtf8("wifiEndpointTitle"));

        wifiLayout->addWidget(wifiEndpointTitle);

        wifiEndpointLabel = new QLabel(wifiTab);
        wifiEndpointLabel->setObjectName(QString::fromUtf8("wifiEndpointLabel"));
        wifiEndpointLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        wifiEndpointLabel->setWordWrap(true);

        wifiLayout->addWidget(wifiEndpointLabel);

        wifiTokenTitle = new QLabel(wifiTab);
        wifiTokenTitle->setObjectName(QString::fromUtf8("wifiTokenTitle"));

        wifiLayout->addWidget(wifiTokenTitle);

        wifiTokenLabel = new QLabel(wifiTab);
        wifiTokenLabel->setObjectName(QString::fromUtf8("wifiTokenLabel"));
        wifiTokenLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        wifiLayout->addWidget(wifiTokenLabel);

        wifiStatusLabel = new QLabel(wifiTab);
        wifiStatusLabel->setObjectName(QString::fromUtf8("wifiStatusLabel"));
        wifiStatusLabel->setWordWrap(true);

        wifiLayout->addWidget(wifiStatusLabel);

        wifiCopyBtn = new QPushButton(wifiTab);
        wifiCopyBtn->setObjectName(QString::fromUtf8("wifiCopyBtn"));

        wifiLayout->addWidget(wifiCopyBtn);

        bleDeviceTitle = new QLabel(wifiTab);
        bleDeviceTitle->setObjectName(QString::fromUtf8("bleDeviceTitle"));

        wifiLayout->addWidget(bleDeviceTitle);

        bleDeviceLabel = new QLabel(wifiTab);
        bleDeviceLabel->setObjectName(QString::fromUtf8("bleDeviceLabel"));
        bleDeviceLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        wifiLayout->addWidget(bleDeviceLabel);

        bleStatusLabel = new QLabel(wifiTab);
        bleStatusLabel->setObjectName(QString::fromUtf8("bleStatusLabel"));
        bleStatusLabel->setWordWrap(true);

        wifiLayout->addWidget(bleStatusLabel);

        wifiSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        wifiLayout->addItem(wifiSpacer);

        modeTabWidget->addTab(wifiTab, QString());
        qrTab = new QWidget();
        qrTab->setObjectName(QString::fromUtf8("qrTab"));
        qrLayout = new QVBoxLayout(qrTab);
        qrLayout->setObjectName(QString::fromUtf8("qrLayout"));
        qrImageLabel = new QLabel(qrTab);
        qrImageLabel->setObjectName(QString::fromUtf8("qrImageLabel"));
        qrImageLabel->setMinimumSize(QSize(260, 260));
        qrImageLabel->setFrameShape(QFrame::Box);
        qrImageLabel->setAlignment(Qt::AlignCenter);

        qrLayout->addWidget(qrImageLabel);

        qrIpRow = new QHBoxLayout();
        qrIpRow->setObjectName(QString::fromUtf8("qrIpRow"));
        qrIpTitle = new QLabel(qrTab);
        qrIpTitle->setObjectName(QString::fromUtf8("qrIpTitle"));

        qrIpRow->addWidget(qrIpTitle);

        qrIpCombo = new QComboBox(qrTab);
        qrIpCombo->setObjectName(QString::fromUtf8("qrIpCombo"));

        qrIpRow->addWidget(qrIpCombo);


        qrLayout->addLayout(qrIpRow);

        qrUrlLabel = new QLabel(qrTab);
        qrUrlLabel->setObjectName(QString::fromUtf8("qrUrlLabel"));
        qrUrlLabel->setWordWrap(true);
        qrUrlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        qrLayout->addWidget(qrUrlLabel);

        qrStatusLabel = new QLabel(qrTab);
        qrStatusLabel->setObjectName(QString::fromUtf8("qrStatusLabel"));
        qrStatusLabel->setWordWrap(true);

        qrLayout->addWidget(qrStatusLabel);

        qrBtnRow = new QHBoxLayout();
        qrBtnRow->setObjectName(QString::fromUtf8("qrBtnRow"));
        qrRefreshBtn = new QPushButton(qrTab);
        qrRefreshBtn->setObjectName(QString::fromUtf8("qrRefreshBtn"));

        qrBtnRow->addWidget(qrRefreshBtn);

        qrCopyBtn = new QPushButton(qrTab);
        qrCopyBtn->setObjectName(QString::fromUtf8("qrCopyBtn"));

        qrBtnRow->addWidget(qrCopyBtn);

        qrDisconnectBtn = new QPushButton(qrTab);
        qrDisconnectBtn->setObjectName(QString::fromUtf8("qrDisconnectBtn"));

        qrBtnRow->addWidget(qrDisconnectBtn);


        qrLayout->addLayout(qrBtnRow);

        modeTabWidget->addTab(qrTab, QString());

        verticalLayout->addWidget(modeTabWidget);

        bottomLayout = new QHBoxLayout();
        bottomLayout->setObjectName(QString::fromUtf8("bottomLayout"));
        bottomSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        bottomLayout->addItem(bottomSpacer);

        closeWindowBtn = new QPushButton(RemoteControlCenter);
        closeWindowBtn->setObjectName(QString::fromUtf8("closeWindowBtn"));

        bottomLayout->addWidget(closeWindowBtn);


        verticalLayout->addLayout(bottomLayout);


        retranslateUi(RemoteControlCenter);

        modeTabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(RemoteControlCenter);
    } // setupUi

    void retranslateUi(QDialog *RemoteControlCenter)
    {
        RemoteControlCenter->setWindowTitle(QCoreApplication::translate("RemoteControlCenter", "\350\277\234\347\250\213\346\216\247\345\210\266", nullptr));
        serviceGroup->setTitle(QCoreApplication::translate("RemoteControlCenter", "\350\277\234\347\250\213\346\234\215\345\212\241", nullptr));
        remoteOffRadio->setText(QCoreApplication::translate("RemoteControlCenter", "\345\205\263\351\227\255", nullptr));
        remoteOnRadio->setText(QCoreApplication::translate("RemoteControlCenter", "\345\274\200\345\220\257", nullptr));
        serviceSummaryLabel->setText(QCoreApplication::translate("RemoteControlCenter", "\346\234\252\345\220\257\347\224\250", nullptr));
        wifiHintLabel->setText(QCoreApplication::translate("RemoteControlCenter", "WiFi\357\274\232\345\256\242\346\210\267\347\253\257\344\270\216\344\270\273\346\234\272\351\241\273\345\244\204\344\272\216\345\220\214\344\270\200\345\261\200\345\237\237\347\275\221\357\274\214\345\234\250\345\260\217\347\250\213\345\272\217 WiFi \351\241\265\345\241\253\345\206\231\344\270\213\345\210\227\345\234\260\345\235\200\344\270\216 token\343\200\202\350\223\235\347\211\231\357\274\232\345\234\250\345\260\217\347\250\213\345\272\217 BLE \351\241\265\346\211\253\346\217\217\350\277\236\346\216\245\344\270\213\345\210\227\350\256\276\345\244\207\345\220\215\357\274\210\351\242\204\350\247\210\345\233\276\345\203\217\344\273\205 WiFi \351\200\232\351\201\223\357\274\211\343\200\202", nullptr));
        wifiEndpointTitle->setText(QCoreApplication::translate("RemoteControlCenter", "HTTP \345\234\260\345\235\200\357\274\232", nullptr));
        wifiEndpointLabel->setText(QCoreApplication::translate("RemoteControlCenter", "\342\200\224", nullptr));
        wifiTokenTitle->setText(QCoreApplication::translate("RemoteControlCenter", "\350\256\244\350\257\201 token\357\274\232", nullptr));
        wifiTokenLabel->setText(QCoreApplication::translate("RemoteControlCenter", "\342\200\224", nullptr));
        wifiStatusLabel->setText(QCoreApplication::translate("RemoteControlCenter", "HTTP \347\212\266\346\200\201\357\274\232\342\200\224", nullptr));
        wifiCopyBtn->setText(QCoreApplication::translate("RemoteControlCenter", "\345\244\215\345\210\266 HTTP \345\234\260\345\235\200", nullptr));
        bleDeviceTitle->setText(QCoreApplication::translate("RemoteControlCenter", "BLE \345\271\277\346\222\255\350\256\276\345\244\207\345\220\215\357\274\232", nullptr));
        bleDeviceLabel->setText(QCoreApplication::translate("RemoteControlCenter", "\342\200\224", nullptr));
        bleStatusLabel->setText(QCoreApplication::translate("RemoteControlCenter", "BLE \347\212\266\346\200\201\357\274\232\342\200\224", nullptr));
        modeTabWidget->setTabText(modeTabWidget->indexOf(wifiTab), QCoreApplication::translate("RemoteControlCenter", "\345\260\217\347\250\213\345\272\217", nullptr));
        qrImageLabel->setText(QString());
        qrIpTitle->setText(QCoreApplication::translate("RemoteControlCenter", "\346\234\254\346\234\272 IP\357\274\232", nullptr));
        qrUrlLabel->setText(QCoreApplication::translate("RemoteControlCenter", "\350\256\277\351\227\256 URL\357\274\232\342\200\224", nullptr));
        qrStatusLabel->setText(QCoreApplication::translate("RemoteControlCenter", "\346\211\253\347\240\201\347\212\266\346\200\201\357\274\232\342\200\224", nullptr));
        qrRefreshBtn->setText(QCoreApplication::translate("RemoteControlCenter", "\345\210\267\346\226\260\344\274\232\350\257\235", nullptr));
        qrCopyBtn->setText(QCoreApplication::translate("RemoteControlCenter", "\345\244\215\345\210\266\351\223\276\346\216\245", nullptr));
        qrDisconnectBtn->setText(QCoreApplication::translate("RemoteControlCenter", "\347\273\223\346\235\237\346\211\253\347\240\201", nullptr));
        modeTabWidget->setTabText(modeTabWidget->indexOf(qrTab), QCoreApplication::translate("RemoteControlCenter", "\346\211\253\347\240\201\347\275\221\351\241\265", nullptr));
        closeWindowBtn->setText(QCoreApplication::translate("RemoteControlCenter", "\345\205\263\351\227\255\347\252\227\345\217\243", nullptr));
    } // retranslateUi

};

namespace Ui {
    class RemoteControlCenter: public Ui_RemoteControlCenter {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_REMOTECONTROLCENTER_H
