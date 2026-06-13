/********************************************************************************
** Form generated from reading UI file 'QtProject_1.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_QTPROJECT_1_H
#define UI_QTPROJECT_1_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <ui/PreviewWidget.h>

QT_BEGIN_NAMESPACE

class Ui_QtProject_1Class
{
public:
    QHBoxLayout *mainLayout;
    QSplitter *mainSplitter;
    QGroupBox *previewGroup;
    QVBoxLayout *previewLayout;
    PreviewWidget *imageLabel;
    QLabel *previewInfoLabel;
    QWidget *rightPanel;
    QVBoxLayout *rightLayout;
    QTabWidget *rightTabWidget;
    QWidget *captureTab;
    QVBoxLayout *captureTabLayout;
    QGroupBox *cameraParamGroup;
    QFormLayout *cameraParamForm;
    QLabel *exposureLabel;
    QDoubleSpinBox *exposureSpin;
    QLabel *gainLabel;
    QDoubleSpinBox *gainSpin;
    QLabel *fpsLabel;
    QDoubleSpinBox *fpsSpin;
    QLabel *paramRangeLabel;
    QGroupBox *cameraActionGroup;
    QGridLayout *cameraActionGrid;
    QLabel *cameraSelectLabel;
    QComboBox *cameraSelectCombo;
    QPushButton *openCameraBtn;
    QPushButton *closeCameraBtn;
    QPushButton *startGrabBtn;
    QPushButton *stopGrabBtn;
    QPushButton *applyParamBtn;
    QPushButton *saveOneBmpBtn;
    QLabel *cameraStatusLabel;
    QSpacerItem *captureTabSpacer;
    QWidget *stageSaveTab;
    QVBoxLayout *stageSaveTabLayout;
    QGroupBox *stageGroup;
    QVBoxLayout *stageLayout;
    QLabel *stageStatusLabel;
    QTableWidget *stageTable;
    QVBoxLayout *stageBtnOuterLayout;
    QHBoxLayout *stageBtnRow1;
    QPushButton *addStageBtn;
    QPushButton *deleteStageBtn;
    QPushButton *clearStageBtn;
    QLabel *loopCountLabel;
    QSpinBox *loopCountSpin;
    QSpacerItem *stageBtnRow1Spacer;
    QHBoxLayout *stageBtnRow2;
    QPushButton *startStageBtn;
    QPushButton *stopStageBtn;
    QSpacerItem *stageBtnRow2Spacer;
    QGroupBox *saveSettingGroup;
    QFormLayout *saveSettingForm;
    QLabel *savePathLabel;
    QHBoxLayout *savePathLayout;
    QLineEdit *savePathEdit;
    QPushButton *browsePathBtn;
    QLabel *saveModeLabel;
    QComboBox *saveModeCombo;
    QLabel *picsPerFolderLabel;
    QSpinBox *picsPerFolderSpin;
    QLabel *secondsPerFolderLabel;
    QSpinBox *secondsPerFolderSpin;
    QLabel *maxSaveCountLabel;
    QSpinBox *maxSaveCountSpin;
    QWidget *logTab;
    QVBoxLayout *logTabLayout;
    QGroupBox *logGroup;
    QVBoxLayout *logLayout;
    QPlainTextEdit *logTextEdit;
    QPushButton *clearLogBtn;

    void setupUi(QWidget *QtProject_1Class)
    {
        if (QtProject_1Class->objectName().isEmpty())
            QtProject_1Class->setObjectName(QString::fromUtf8("QtProject_1Class"));
        QtProject_1Class->resize(1280, 800);
        mainLayout = new QHBoxLayout(QtProject_1Class);
        mainLayout->setSpacing(8);
        mainLayout->setObjectName(QString::fromUtf8("mainLayout"));
        mainLayout->setContentsMargins(8, 8, 8, 8);
        mainSplitter = new QSplitter(QtProject_1Class);
        mainSplitter->setObjectName(QString::fromUtf8("mainSplitter"));
        mainSplitter->setOrientation(Qt::Horizontal);
        previewGroup = new QGroupBox(mainSplitter);
        previewGroup->setObjectName(QString::fromUtf8("previewGroup"));
        previewLayout = new QVBoxLayout(previewGroup);
        previewLayout->setObjectName(QString::fromUtf8("previewLayout"));
        imageLabel = new PreviewWidget(previewGroup);
        imageLabel->setObjectName(QString::fromUtf8("imageLabel"));
        imageLabel->setMinimumSize(QSize(640, 512));

        previewLayout->addWidget(imageLabel);

        previewInfoLabel = new QLabel(previewGroup);
        previewInfoLabel->setObjectName(QString::fromUtf8("previewInfoLabel"));

        previewLayout->addWidget(previewInfoLabel);

        previewLayout->setStretch(0, 3);
        mainSplitter->addWidget(previewGroup);
        rightPanel = new QWidget(mainSplitter);
        rightPanel->setObjectName(QString::fromUtf8("rightPanel"));
        rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setObjectName(QString::fromUtf8("rightLayout"));
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightTabWidget = new QTabWidget(rightPanel);
        rightTabWidget->setObjectName(QString::fromUtf8("rightTabWidget"));
        rightTabWidget->setDocumentMode(true);
        captureTab = new QWidget();
        captureTab->setObjectName(QString::fromUtf8("captureTab"));
        captureTabLayout = new QVBoxLayout(captureTab);
        captureTabLayout->setSpacing(8);
        captureTabLayout->setObjectName(QString::fromUtf8("captureTabLayout"));
        cameraParamGroup = new QGroupBox(captureTab);
        cameraParamGroup->setObjectName(QString::fromUtf8("cameraParamGroup"));
        cameraParamForm = new QFormLayout(cameraParamGroup);
        cameraParamForm->setObjectName(QString::fromUtf8("cameraParamForm"));
        exposureLabel = new QLabel(cameraParamGroup);
        exposureLabel->setObjectName(QString::fromUtf8("exposureLabel"));

        cameraParamForm->setWidget(0, QFormLayout::LabelRole, exposureLabel);

        exposureSpin = new QDoubleSpinBox(cameraParamGroup);
        exposureSpin->setObjectName(QString::fromUtf8("exposureSpin"));
        exposureSpin->setDecimals(0);
        exposureSpin->setMaximum(1000000.000000000000000);
        exposureSpin->setValue(10000.000000000000000);

        cameraParamForm->setWidget(0, QFormLayout::FieldRole, exposureSpin);

        gainLabel = new QLabel(cameraParamGroup);
        gainLabel->setObjectName(QString::fromUtf8("gainLabel"));

        cameraParamForm->setWidget(1, QFormLayout::LabelRole, gainLabel);

        gainSpin = new QDoubleSpinBox(cameraParamGroup);
        gainSpin->setObjectName(QString::fromUtf8("gainSpin"));
        gainSpin->setDecimals(1);
        gainSpin->setMaximum(24.000000000000000);

        cameraParamForm->setWidget(1, QFormLayout::FieldRole, gainSpin);

        fpsLabel = new QLabel(cameraParamGroup);
        fpsLabel->setObjectName(QString::fromUtf8("fpsLabel"));

        cameraParamForm->setWidget(2, QFormLayout::LabelRole, fpsLabel);

        fpsSpin = new QDoubleSpinBox(cameraParamGroup);
        fpsSpin->setObjectName(QString::fromUtf8("fpsSpin"));
        fpsSpin->setDecimals(1);
        fpsSpin->setMinimum(1.000000000000000);
        fpsSpin->setMaximum(120.000000000000000);
        fpsSpin->setValue(20.000000000000000);

        cameraParamForm->setWidget(2, QFormLayout::FieldRole, fpsSpin);

        paramRangeLabel = new QLabel(cameraParamGroup);
        paramRangeLabel->setObjectName(QString::fromUtf8("paramRangeLabel"));
        paramRangeLabel->setWordWrap(true);

        cameraParamForm->setWidget(3, QFormLayout::SpanningRole, paramRangeLabel);


        captureTabLayout->addWidget(cameraParamGroup);

        cameraActionGroup = new QGroupBox(captureTab);
        cameraActionGroup->setObjectName(QString::fromUtf8("cameraActionGroup"));
        cameraActionGrid = new QGridLayout(cameraActionGroup);
        cameraActionGrid->setObjectName(QString::fromUtf8("cameraActionGrid"));
        cameraSelectLabel = new QLabel(cameraActionGroup);
        cameraSelectLabel->setObjectName(QString::fromUtf8("cameraSelectLabel"));

        cameraActionGrid->addWidget(cameraSelectLabel, 0, 0, 1, 1);

        cameraSelectCombo = new QComboBox(cameraActionGroup);
        cameraSelectCombo->setObjectName(QString::fromUtf8("cameraSelectCombo"));

        cameraActionGrid->addWidget(cameraSelectCombo, 0, 1, 1, 1);

        openCameraBtn = new QPushButton(cameraActionGroup);
        openCameraBtn->setObjectName(QString::fromUtf8("openCameraBtn"));

        cameraActionGrid->addWidget(openCameraBtn, 1, 0, 1, 1);

        closeCameraBtn = new QPushButton(cameraActionGroup);
        closeCameraBtn->setObjectName(QString::fromUtf8("closeCameraBtn"));
        closeCameraBtn->setEnabled(false);

        cameraActionGrid->addWidget(closeCameraBtn, 1, 1, 1, 1);

        startGrabBtn = new QPushButton(cameraActionGroup);
        startGrabBtn->setObjectName(QString::fromUtf8("startGrabBtn"));
        startGrabBtn->setEnabled(false);

        cameraActionGrid->addWidget(startGrabBtn, 2, 0, 1, 1);

        stopGrabBtn = new QPushButton(cameraActionGroup);
        stopGrabBtn->setObjectName(QString::fromUtf8("stopGrabBtn"));
        stopGrabBtn->setEnabled(false);

        cameraActionGrid->addWidget(stopGrabBtn, 2, 1, 1, 1);

        applyParamBtn = new QPushButton(cameraActionGroup);
        applyParamBtn->setObjectName(QString::fromUtf8("applyParamBtn"));
        applyParamBtn->setEnabled(false);

        cameraActionGrid->addWidget(applyParamBtn, 3, 0, 1, 1);

        saveOneBmpBtn = new QPushButton(cameraActionGroup);
        saveOneBmpBtn->setObjectName(QString::fromUtf8("saveOneBmpBtn"));
        saveOneBmpBtn->setEnabled(false);

        cameraActionGrid->addWidget(saveOneBmpBtn, 3, 1, 1, 1);

        cameraStatusLabel = new QLabel(cameraActionGroup);
        cameraStatusLabel->setObjectName(QString::fromUtf8("cameraStatusLabel"));
        cameraStatusLabel->setWordWrap(true);

        cameraActionGrid->addWidget(cameraStatusLabel, 4, 0, 1, 2);


        captureTabLayout->addWidget(cameraActionGroup);

        captureTabSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        captureTabLayout->addItem(captureTabSpacer);

        rightTabWidget->addTab(captureTab, QString());
        stageSaveTab = new QWidget();
        stageSaveTab->setObjectName(QString::fromUtf8("stageSaveTab"));
        stageSaveTabLayout = new QVBoxLayout(stageSaveTab);
        stageSaveTabLayout->setSpacing(8);
        stageSaveTabLayout->setObjectName(QString::fromUtf8("stageSaveTabLayout"));
        stageGroup = new QGroupBox(stageSaveTab);
        stageGroup->setObjectName(QString::fromUtf8("stageGroup"));
        stageLayout = new QVBoxLayout(stageGroup);
        stageLayout->setObjectName(QString::fromUtf8("stageLayout"));
        stageStatusLabel = new QLabel(stageGroup);
        stageStatusLabel->setObjectName(QString::fromUtf8("stageStatusLabel"));
        stageStatusLabel->setWordWrap(true);

        stageLayout->addWidget(stageStatusLabel);

        stageTable = new QTableWidget(stageGroup);
        stageTable->setObjectName(QString::fromUtf8("stageTable"));

        stageLayout->addWidget(stageTable);

        stageBtnOuterLayout = new QVBoxLayout();
        stageBtnOuterLayout->setSpacing(6);
        stageBtnOuterLayout->setObjectName(QString::fromUtf8("stageBtnOuterLayout"));
        stageBtnRow1 = new QHBoxLayout();
        stageBtnRow1->setObjectName(QString::fromUtf8("stageBtnRow1"));
        addStageBtn = new QPushButton(stageGroup);
        addStageBtn->setObjectName(QString::fromUtf8("addStageBtn"));

        stageBtnRow1->addWidget(addStageBtn);

        deleteStageBtn = new QPushButton(stageGroup);
        deleteStageBtn->setObjectName(QString::fromUtf8("deleteStageBtn"));

        stageBtnRow1->addWidget(deleteStageBtn);

        clearStageBtn = new QPushButton(stageGroup);
        clearStageBtn->setObjectName(QString::fromUtf8("clearStageBtn"));

        stageBtnRow1->addWidget(clearStageBtn);

        loopCountLabel = new QLabel(stageGroup);
        loopCountLabel->setObjectName(QString::fromUtf8("loopCountLabel"));

        stageBtnRow1->addWidget(loopCountLabel);

        loopCountSpin = new QSpinBox(stageGroup);
        loopCountSpin->setObjectName(QString::fromUtf8("loopCountSpin"));
        loopCountSpin->setMinimum(1);
        loopCountSpin->setValue(1);

        stageBtnRow1->addWidget(loopCountSpin);

        stageBtnRow1Spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        stageBtnRow1->addItem(stageBtnRow1Spacer);


        stageBtnOuterLayout->addLayout(stageBtnRow1);

        stageBtnRow2 = new QHBoxLayout();
        stageBtnRow2->setObjectName(QString::fromUtf8("stageBtnRow2"));
        startStageBtn = new QPushButton(stageGroup);
        startStageBtn->setObjectName(QString::fromUtf8("startStageBtn"));
        startStageBtn->setEnabled(false);

        stageBtnRow2->addWidget(startStageBtn);

        stopStageBtn = new QPushButton(stageGroup);
        stopStageBtn->setObjectName(QString::fromUtf8("stopStageBtn"));
        stopStageBtn->setEnabled(false);

        stageBtnRow2->addWidget(stopStageBtn);

        stageBtnRow2Spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        stageBtnRow2->addItem(stageBtnRow2Spacer);

        stageBtnRow2->setStretch(0, 1);
        stageBtnRow2->setStretch(1, 1);
        stageBtnRow2->setStretch(2, 1);

        stageBtnOuterLayout->addLayout(stageBtnRow2);


        stageLayout->addLayout(stageBtnOuterLayout);


        stageSaveTabLayout->addWidget(stageGroup);

        saveSettingGroup = new QGroupBox(stageSaveTab);
        saveSettingGroup->setObjectName(QString::fromUtf8("saveSettingGroup"));
        saveSettingForm = new QFormLayout(saveSettingGroup);
        saveSettingForm->setObjectName(QString::fromUtf8("saveSettingForm"));
        savePathLabel = new QLabel(saveSettingGroup);
        savePathLabel->setObjectName(QString::fromUtf8("savePathLabel"));

        saveSettingForm->setWidget(0, QFormLayout::LabelRole, savePathLabel);

        savePathLayout = new QHBoxLayout();
        savePathLayout->setObjectName(QString::fromUtf8("savePathLayout"));
        savePathEdit = new QLineEdit(saveSettingGroup);
        savePathEdit->setObjectName(QString::fromUtf8("savePathEdit"));

        savePathLayout->addWidget(savePathEdit);

        browsePathBtn = new QPushButton(saveSettingGroup);
        browsePathBtn->setObjectName(QString::fromUtf8("browsePathBtn"));

        savePathLayout->addWidget(browsePathBtn);


        saveSettingForm->setLayout(0, QFormLayout::FieldRole, savePathLayout);

        saveModeLabel = new QLabel(saveSettingGroup);
        saveModeLabel->setObjectName(QString::fromUtf8("saveModeLabel"));

        saveSettingForm->setWidget(1, QFormLayout::LabelRole, saveModeLabel);

        saveModeCombo = new QComboBox(saveSettingGroup);
        saveModeCombo->addItem(QString());
        saveModeCombo->addItem(QString());
        saveModeCombo->addItem(QString());
        saveModeCombo->setObjectName(QString::fromUtf8("saveModeCombo"));

        saveSettingForm->setWidget(1, QFormLayout::FieldRole, saveModeCombo);

        picsPerFolderLabel = new QLabel(saveSettingGroup);
        picsPerFolderLabel->setObjectName(QString::fromUtf8("picsPerFolderLabel"));

        saveSettingForm->setWidget(2, QFormLayout::LabelRole, picsPerFolderLabel);

        picsPerFolderSpin = new QSpinBox(saveSettingGroup);
        picsPerFolderSpin->setObjectName(QString::fromUtf8("picsPerFolderSpin"));
        picsPerFolderSpin->setMinimum(1);
        picsPerFolderSpin->setMaximum(100000);
        picsPerFolderSpin->setValue(100);

        saveSettingForm->setWidget(2, QFormLayout::FieldRole, picsPerFolderSpin);

        secondsPerFolderLabel = new QLabel(saveSettingGroup);
        secondsPerFolderLabel->setObjectName(QString::fromUtf8("secondsPerFolderLabel"));

        saveSettingForm->setWidget(3, QFormLayout::LabelRole, secondsPerFolderLabel);

        secondsPerFolderSpin = new QSpinBox(saveSettingGroup);
        secondsPerFolderSpin->setObjectName(QString::fromUtf8("secondsPerFolderSpin"));
        secondsPerFolderSpin->setMinimum(1);
        secondsPerFolderSpin->setValue(10);

        saveSettingForm->setWidget(3, QFormLayout::FieldRole, secondsPerFolderSpin);

        maxSaveCountLabel = new QLabel(saveSettingGroup);
        maxSaveCountLabel->setObjectName(QString::fromUtf8("maxSaveCountLabel"));

        saveSettingForm->setWidget(4, QFormLayout::LabelRole, maxSaveCountLabel);

        maxSaveCountSpin = new QSpinBox(saveSettingGroup);
        maxSaveCountSpin->setObjectName(QString::fromUtf8("maxSaveCountSpin"));
        maxSaveCountSpin->setMaximum(9999999);

        saveSettingForm->setWidget(4, QFormLayout::FieldRole, maxSaveCountSpin);


        stageSaveTabLayout->addWidget(saveSettingGroup);

        rightTabWidget->addTab(stageSaveTab, QString());
        logTab = new QWidget();
        logTab->setObjectName(QString::fromUtf8("logTab"));
        logTabLayout = new QVBoxLayout(logTab);
        logTabLayout->setObjectName(QString::fromUtf8("logTabLayout"));
        logGroup = new QGroupBox(logTab);
        logGroup->setObjectName(QString::fromUtf8("logGroup"));
        logLayout = new QVBoxLayout(logGroup);
        logLayout->setObjectName(QString::fromUtf8("logLayout"));
        logTextEdit = new QPlainTextEdit(logGroup);
        logTextEdit->setObjectName(QString::fromUtf8("logTextEdit"));
        logTextEdit->setReadOnly(true);
        logTextEdit->setMaximumBlockCount(2000);

        logLayout->addWidget(logTextEdit);

        clearLogBtn = new QPushButton(logGroup);
        clearLogBtn->setObjectName(QString::fromUtf8("clearLogBtn"));

        logLayout->addWidget(clearLogBtn);


        logTabLayout->addWidget(logGroup);

        rightTabWidget->addTab(logTab, QString());

        rightLayout->addWidget(rightTabWidget);

        mainSplitter->addWidget(rightPanel);

        mainLayout->addWidget(mainSplitter);


        retranslateUi(QtProject_1Class);

        rightTabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(QtProject_1Class);
    } // setupUi

    void retranslateUi(QWidget *QtProject_1Class)
    {
        QtProject_1Class->setWindowTitle(QCoreApplication::translate("QtProject_1Class", "Basler \347\233\270\346\234\272\351\207\207\351\233\206", nullptr));
        previewGroup->setTitle(QCoreApplication::translate("QtProject_1Class", "\345\233\276\345\203\217\351\242\204\350\247\210 (2592 x 2048)", nullptr));
        previewInfoLabel->setText(QCoreApplication::translate("QtProject_1Class", "\346\230\276\347\244\272\345\210\267\346\226\260: 50ms", nullptr));
        cameraParamGroup->setTitle(QCoreApplication::translate("QtProject_1Class", "\347\233\270\346\234\272\345\217\202\346\225\260", nullptr));
        exposureLabel->setText(QCoreApplication::translate("QtProject_1Class", "\346\233\235\345\205\211 (us)", nullptr));
        gainLabel->setText(QCoreApplication::translate("QtProject_1Class", "\345\242\236\347\233\212 (dB)", nullptr));
        fpsLabel->setText(QCoreApplication::translate("QtProject_1Class", "\345\270\247\347\216\207 (fps)", nullptr));
        paramRangeLabel->setText(QCoreApplication::translate("QtProject_1Class", "\345\217\202\346\225\260\350\214\203\345\233\264: \350\277\236\346\216\245\347\233\270\346\234\272\345\220\216\346\233\264\346\226\260", nullptr));
        cameraActionGroup->setTitle(QCoreApplication::translate("QtProject_1Class", "\347\233\270\346\234\272\346\223\215\344\275\234", nullptr));
        cameraSelectLabel->setText(QCoreApplication::translate("QtProject_1Class", "\347\233\270\346\234\272\350\256\276\345\244\207", nullptr));
        openCameraBtn->setText(QCoreApplication::translate("QtProject_1Class", "\346\211\223\345\274\200\347\233\270\346\234\272", nullptr));
        closeCameraBtn->setText(QCoreApplication::translate("QtProject_1Class", "\345\205\263\351\227\255\347\233\270\346\234\272", nullptr));
        startGrabBtn->setText(QCoreApplication::translate("QtProject_1Class", "\345\274\200\345\247\213\351\207\207\351\233\206", nullptr));
        stopGrabBtn->setText(QCoreApplication::translate("QtProject_1Class", "\345\201\234\346\255\242\351\207\207\351\233\206", nullptr));
        applyParamBtn->setText(QCoreApplication::translate("QtProject_1Class", "\345\272\224\347\224\250\345\217\202\346\225\260", nullptr));
        saveOneBmpBtn->setText(QCoreApplication::translate("QtProject_1Class", "\344\277\235\345\255\230\345\215\225\345\274\240 BMP", nullptr));
        cameraStatusLabel->setText(QCoreApplication::translate("QtProject_1Class", "\347\212\266\346\200\201: \346\234\252\350\277\236\346\216\245", nullptr));
        rightTabWidget->setTabText(rightTabWidget->indexOf(captureTab), QCoreApplication::translate("QtProject_1Class", "\351\207\207\351\233\206\346\216\247\345\210\266", nullptr));
        stageGroup->setTitle(QCoreApplication::translate("QtProject_1Class", "\351\230\266\346\256\265\351\207\207\351\233\206", nullptr));
        stageStatusLabel->setText(QCoreApplication::translate("QtProject_1Class", "\345\275\223\345\211\215\346\227\240\350\277\220\350\241\214\344\270\255\347\232\204\351\230\266\346\256\265", nullptr));
        addStageBtn->setText(QCoreApplication::translate("QtProject_1Class", "\346\226\260\345\242\236", nullptr));
        deleteStageBtn->setText(QCoreApplication::translate("QtProject_1Class", "\345\210\240\351\231\244", nullptr));
        clearStageBtn->setText(QCoreApplication::translate("QtProject_1Class", "\346\270\205\347\251\272", nullptr));
        loopCountLabel->setText(QCoreApplication::translate("QtProject_1Class", "\345\276\252\347\216\257", nullptr));
        startStageBtn->setText(QCoreApplication::translate("QtProject_1Class", "\345\274\200\345\247\213\351\230\266\346\256\265\351\207\207\351\233\206", nullptr));
        stopStageBtn->setText(QCoreApplication::translate("QtProject_1Class", "\345\201\234\346\255\242\351\230\266\346\256\265", nullptr));
        saveSettingGroup->setTitle(QCoreApplication::translate("QtProject_1Class", "\345\255\230\345\233\276\350\256\276\347\275\256", nullptr));
        savePathLabel->setText(QCoreApplication::translate("QtProject_1Class", "\344\277\235\345\255\230\350\267\257\345\276\204", nullptr));
        browsePathBtn->setText(QCoreApplication::translate("QtProject_1Class", "\346\265\217\350\247\210", nullptr));
        saveModeLabel->setText(QCoreApplication::translate("QtProject_1Class", "\345\255\230\345\233\276\346\250\241\345\274\217", nullptr));
        saveModeCombo->setItemText(0, QCoreApplication::translate("QtProject_1Class", "\345\215\225\346\226\207\344\273\266\345\244\271", nullptr));
        saveModeCombo->setItemText(1, QCoreApplication::translate("QtProject_1Class", "\346\214\211\345\274\240\346\225\260\345\210\206\346\226\207\344\273\266\345\244\271", nullptr));
        saveModeCombo->setItemText(2, QCoreApplication::translate("QtProject_1Class", "\346\214\211\346\227\266\351\227\264\345\210\206\346\226\207\344\273\266\345\244\271", nullptr));

        picsPerFolderLabel->setText(QCoreApplication::translate("QtProject_1Class", "\346\257\217\346\226\207\344\273\266\345\244\271\345\274\240\346\225\260", nullptr));
        secondsPerFolderLabel->setText(QCoreApplication::translate("QtProject_1Class", "\346\257\217\346\226\207\344\273\266\345\244\271\347\247\222\346\225\260", nullptr));
        maxSaveCountLabel->setText(QCoreApplication::translate("QtProject_1Class", "\346\200\273\344\277\235\345\255\230\344\270\212\351\231\220(0\344\270\215\351\231\220)", nullptr));
        rightTabWidget->setTabText(rightTabWidget->indexOf(stageSaveTab), QCoreApplication::translate("QtProject_1Class", "\351\230\266\346\256\265\344\270\216\345\255\230\345\233\276", nullptr));
        logGroup->setTitle(QCoreApplication::translate("QtProject_1Class", "\350\277\220\350\241\214\346\227\245\345\277\227", nullptr));
        clearLogBtn->setText(QCoreApplication::translate("QtProject_1Class", "\346\270\205\347\251\272\346\227\245\345\277\227", nullptr));
        rightTabWidget->setTabText(rightTabWidget->indexOf(logTab), QCoreApplication::translate("QtProject_1Class", "\346\227\245\345\277\227", nullptr));
    } // retranslateUi

};

namespace Ui {
    class QtProject_1Class: public Ui_QtProject_1Class {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_QTPROJECT_1_H
