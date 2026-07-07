#include "RecorderNavSidebar.h"
#include "ui_RecorderNavSidebar.h"

#include <QButtonGroup>
#include <QPushButton>

RecorderNavSidebar::RecorderNavSidebar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::RecorderNavSidebar)
{
    ui->setupUi(this);

    ui->settingsBtn->setCheckable(true);

    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    group->addButton(ui->generalBtn, General);
    group->addButton(ui->settingsBtn, Settings);
    group->addButton(ui->listBtn, List);

    connect(ui->generalBtn, &QPushButton::clicked, this, [this]() { selectPage(General); });
    connect(ui->settingsBtn, &QPushButton::clicked, this, [this]() { selectPage(Settings); });
    connect(ui->listBtn, &QPushButton::clicked, this, [this]() { selectPage(List); });

    setCurrentPage(General);
}

RecorderNavSidebar::~RecorderNavSidebar()
{
    delete ui;
}

void RecorderNavSidebar::setCurrentPage(Page page)
{
    m_current = page;
    ui->generalBtn->setChecked(page == General);
    ui->settingsBtn->setChecked(page == Settings);
    ui->listBtn->setChecked(page == List);
}

void RecorderNavSidebar::setSettingsEnabled(bool enabled)
{
    ui->settingsBtn->setEnabled(enabled);
}

void RecorderNavSidebar::selectPage(Page page)
{
    if (m_current == page)
        return;
    m_current = page;
    ui->generalBtn->setChecked(page == General);
    ui->settingsBtn->setChecked(page == Settings);
    ui->listBtn->setChecked(page == List);
    emit pageChanged(page);
}
