#include "RecorderOutputListWidget.h"
#include "ui_RecorderOutputListWidget.h"
#include "RecorderStatusText.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QTableWidget>
#include <QUrl>

namespace
{

QString formatSize(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024);
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    return QStringLiteral("%1 M").arg(QString::number(mb, 'f', 2));
}

QString formatSavedAt(const QString &iso)
{
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid())
        return QStringLiteral("—");
    return dt.toString(QStringLiteral("yyyy/MM/dd HH:mm"));
}

QTableWidgetItem *centeredTableItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

} // namespace

RecorderOutputListWidget::RecorderOutputListWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::RecorderOutputListWidget)
{
    ui->setupUi(this);

    ui->outputTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->outputTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->outputTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->outputTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->outputTable->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->outputTable, &QTableWidget::customContextMenuRequested,
            this, &RecorderOutputListWidget::onContextMenuRequested);
    connect(ui->outputTable, &QTableWidget::cellDoubleClicked,
            this, &RecorderOutputListWidget::onItemDoubleClicked);
}

RecorderOutputListWidget::~RecorderOutputListWidget()
{
    delete ui;
}

void RecorderOutputListWidget::setStore(RecorderOutputStore *store)
{
    m_store = store;
}

void RecorderOutputListWidget::reloadList()
{
    refreshTable();
}

void RecorderOutputListWidget::setControlsEnabled(bool enabled)
{
    ui->outputTable->setEnabled(enabled);
}

void RecorderOutputListWidget::refreshTable()
{
    ui->outputTable->setRowCount(0);
    if (!m_store)
        return;

    const QList<RecorderOutputEntry> &entries = m_store->entries();
    ui->outputTable->setRowCount(entries.size());
    for (int i = 0; i < entries.size(); ++i)
    {
        const RecorderOutputEntry &e = entries.at(i);
        const QFileInfo fi(e.filePath);
        ui->outputTable->setItem(i, 0, centeredTableItem(fi.fileName()));
        ui->outputTable->setItem(i, 1, centeredTableItem(RecorderStatusText::formatDuration(e.durationSeconds)));
        ui->outputTable->setItem(i, 2, centeredTableItem(formatSize(e.sizeBytes)));
        ui->outputTable->setItem(i, 3, centeredTableItem(formatSavedAt(e.savedAt)));
    }
}

int RecorderOutputListWidget::selectedRow() const
{
    const QList<QTableWidgetItem *> rows = ui->outputTable->selectedItems();
    if (rows.isEmpty())
        return -1;
    return rows.first()->row();
}

QString RecorderOutputListWidget::selectedPath() const
{
    if (!m_store)
        return QString();
    const int row = selectedRow();
    if (row < 0 || row >= m_store->entries().size())
        return QString();
    return m_store->entries().at(row).filePath;
}

void RecorderOutputListWidget::onContextMenuRequested(const QPoint &pos)
{
    const QModelIndex idx = ui->outputTable->indexAt(pos);
    if (!idx.isValid())
        return;

    QMenu menu(this);
    menu.addAction(QStringLiteral("播放"), this, &RecorderOutputListWidget::onPlay);
    menu.addAction(QStringLiteral("复制"), this, &RecorderOutputListWidget::onCopy);
    menu.addAction(QStringLiteral("重命名"), this, &RecorderOutputListWidget::onRename);
    menu.addAction(QStringLiteral("文件位置"), this, &RecorderOutputListWidget::onOpenFolder);
    menu.addSeparator();
    menu.addAction(QStringLiteral("删除"), this, &RecorderOutputListWidget::onDelete);
    menu.exec(ui->outputTable->viewport()->mapToGlobal(pos));
}

void RecorderOutputListWidget::onItemDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0)
        return;
    ui->outputTable->selectRow(row);
    onPlay();
}

void RecorderOutputListWidget::onPlay()
{
    const QString path = selectedPath();
    if (path.isEmpty())
        return;
    if (!QFileInfo::exists(path))
    {
        QMessageBox::warning(this, QStringLiteral("屏幕录制"), QStringLiteral("文件不存在。"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void RecorderOutputListWidget::onCopy()
{
    const QString path = selectedPath();
    if (path.isEmpty() || !QFileInfo::exists(path))
        return;

    const QFileInfo fi(path);
    const QString dest = QFileDialog::getSaveFileName(this,
                                                      QStringLiteral("复制到"),
                                                      fi.absolutePath() + QLatin1Char('/') + fi.fileName());
    if (dest.isEmpty())
        return;

    if (QFile::exists(dest))
    {
        QMessageBox::warning(this, QStringLiteral("屏幕录制"), QStringLiteral("目标文件已存在。"));
        return;
    }
    if (!QFile::copy(path, dest))
    {
        QMessageBox::warning(this, QStringLiteral("屏幕录制"), QStringLiteral("复制失败。"));
        return;
    }
}

void RecorderOutputListWidget::onRename()
{
    if (!m_store)
        return;
    const int row = selectedRow();
    if (row < 0 || row >= m_store->entries().size())
        return;

    const QString oldPath = m_store->entries().at(row).filePath;
    const QFileInfo fi(oldPath);
    const QString newName = QInputDialog::getText(this,
                                                  QStringLiteral("重命名"),
                                                  QStringLiteral("新文件名："),
                                                  QLineEdit::Normal,
                                                  fi.fileName());
    if (newName.isEmpty() || newName == fi.fileName())
        return;

    const QString newPath = fi.absolutePath() + QLatin1Char('/') + newName;
    QString err;
    if (!m_store->renameEntryAt(row, newPath, &err))
    {
        QMessageBox::warning(this, QStringLiteral("屏幕录制"), err);
        return;
    }
    m_store->save(nullptr);
    refreshTable();
}

void RecorderOutputListWidget::onOpenFolder()
{
    const QString path = selectedPath();
    if (path.isEmpty())
        return;
    const QFileInfo fi(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
}

void RecorderOutputListWidget::onDelete()
{
    if (!m_store)
        return;
    const int row = selectedRow();
    if (row < 0 || row >= m_store->entries().size())
        return;

    const QString path = m_store->entries().at(row).filePath;
    const auto ret = QMessageBox::question(this,
                                           QStringLiteral("删除输出"),
                                           QStringLiteral("确定删除文件？\n%1").arg(path));
    if (ret != QMessageBox::Yes)
        return;

    QString err;
    if (!m_store->removeEntryAt(row, &err))
    {
        QMessageBox::warning(this, QStringLiteral("屏幕录制"), err);
        return;
    }
    m_store->save(nullptr);
    refreshTable();
}

