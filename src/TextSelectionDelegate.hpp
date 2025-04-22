#ifndef TEXTSELECTIONDELEGATE_HPP
#define TEXTSELECTIONDELEGATE_HPP

#include <QStyledItemDelegate>

class TextSelectionDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit TextSelectionDelegate(QObject *parent = nullptr);

    // Override methods needed for custom editing/display
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;

    // Optional: Override paint for custom look when not editing
    // void paint(QPainter *painter, const QStyleOptionViewItem &option,
    //            const QModelIndex &index) const override;
};

#endif // TEXTSELECTIONDELEGATE_HPP
