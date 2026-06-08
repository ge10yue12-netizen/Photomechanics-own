#include "AppConfig.h"

#include "save/SavePathHelper.h"

#include "ui_QtProject_1.h"



#include <QComboBox>

#include <QDoubleSpinBox>

#include <QLineEdit>

#include <QSpinBox>

#include <QTableWidget>

#include <QTableWidgetItem>

#include <QtGlobal>



namespace

{

// 阶段表默认一行（与 setupStageTable / insertStageRow 一致）

void resetStageTableToDefault(Ui::QtProject_1Class &ui)

{

    ui.stageTable->setRowCount(0);

    ui.stageTable->insertRow(0);

    ui.stageTable->setItem(0, 0, new QTableWidgetItem(QStringLiteral("阶段1")));

    ui.stageTable->setItem(0, 1, new QTableWidgetItem(QStringLiteral("1.0")));

    ui.stageTable->setItem(0, 2, new QTableWidgetItem(QStringLiteral("20")));

    auto *chk = new QTableWidgetItem();

    chk->setFlags(chk->flags() | Qt::ItemIsUserCheckable);

    chk->setCheckState(Qt::Checked); // 默认勾选存图，避免只计时不写盘

    ui.stageTable->setItem(0, 3, chk);

}

} // namespace



// 每次启动恢复固定默认值，不读取注册表/配置文件

void AppConfig::loadUi(Ui::QtProject_1Class &ui)

{

    ui.savePathEdit->setText(SavePathHelper::defaultRootPath());



    ui.saveModeCombo->setCurrentIndex(0);

    ui.picsPerFolderSpin->setValue(100);

    ui.secondsPerFolderSpin->setValue(10);

    ui.maxSaveCountSpin->setValue(0);

    ui.loopCountSpin->setValue(1);



    ui.exposureSpin->setValue(10000.0);

    ui.gainSpin->setValue(0.0);

    ui.fpsSpin->setValue(20.0);



    resetStageTableToDefault(ui);

}



// 不再持久化到 QSettings，避免下次启动沿用上一用户/session 的修改

void AppConfig::saveUi(const Ui::QtProject_1Class &ui)

{

    Q_UNUSED(ui);

}


