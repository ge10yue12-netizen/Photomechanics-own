#include "RecorderComboStyle.h"

#include <QComboBox>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>

namespace
{

constexpr int kComboRowHeightPx = 26;
constexpr int kComboClosedHeightPx = 28;

class PaddedComboDelegate : public QStyledItemDelegate
{
public:
    explicit PaddedComboDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(size.height(), kComboRowHeightPx));
        return size;
    }
};

const char *comboStyleSheet()
{
    return R"(
QComboBox {
    padding: 2px 26px 2px 8px;
    min-height: 24px;
    border: 1px solid #c8c8c8;
    border-radius: 2px;
    background: #fff;
    color: #212121;
}
QComboBox:hover {
    border-color: #90caf9;
}
QComboBox:focus {
    border-color: #42a5f5;
}
QComboBox:disabled {
    color: #9e9e9e;
    background: #f5f5f5;
}
QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: top right;
    width: 24px;
    border: none;
    background: transparent;
}
QComboBox::down-arrow {
    width: 0;
    height: 0;
    border-left: 5px solid transparent;
    border-right: 5px solid transparent;
    border-top: 6px solid #2196F3;
    margin-right: 4px;
}
QComboBox::down-arrow:disabled {
    border-top-color: #bdbdbd;
}
QComboBox QAbstractItemView {
    padding: 2px 0;
    border: 1px solid #bdbdbd;
    background: #fff;
    selection-background-color: #1976d2;
    selection-color: #fff;
    outline: 0;
}
QComboBox QAbstractItemView::item {
    min-height: 24px;
    padding: 4px 10px;
}
QComboBox QAbstractItemView::item:hover {
    background-color: #e3f2fd;
    color: #212121;
}
QComboBox QAbstractItemView::item:selected {
    background-color: #1976d2;
    color: #fff;
}
)";
}

} // namespace

void RecorderComboStyle::applyTo(QComboBox *combo)
{
    if (!combo)
        return;

    combo->setMinimumHeight(kComboClosedHeightPx);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    combo->setMaxVisibleItems(12);
    combo->setItemDelegate(new PaddedComboDelegate(combo));
    combo->setStyleSheet(QString::fromUtf8(comboStyleSheet()));
}
