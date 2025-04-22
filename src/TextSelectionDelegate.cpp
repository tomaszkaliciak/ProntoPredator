#include "TextSelectionDelegate.hpp"
#include <QLineEdit>
#include <QApplication> // For style hints if needed

TextSelectionDelegate::TextSelectionDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QWidget *TextSelectionDelegate::createEditor(QWidget *parent,
                                             const QStyleOptionViewItem &/* option */,
                                             const QModelIndex &/* index */) const
{
    // Create a QLineEdit as the editor widget
    QLineEdit *editor = new QLineEdit(parent);
    editor->setFrame(false); // Make it look seamless with the cell
    editor->setReadOnly(true); // We only want selection, not editing
    // Optional: Match background color? Might need style sheets or palette handling
    // editor->setStyleSheet("background: transparent; border: none;");
    return editor;
}

void TextSelectionDelegate::setEditorData(QWidget *editor,
                                          const QModelIndex &index) const
{
    // Get the text from the model for the given index
    QString value = index.model()->data(index, Qt::DisplayRole).toString();

    // Set the text in the QLineEdit editor
    QLineEdit *lineEdit = static_cast<QLineEdit*>(editor);
    lineEdit->setText(value);
    // Optional: Select all text by default when editor opens?
    // lineEdit->selectAll();
}

void TextSelectionDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                         const QModelIndex &index) const
{
    // Since the editor is read-only, we don't need to set data back to the model.
    // If editing were allowed, this is where you'd get the editor's value
    // and use model->setData(index, value, Qt::EditRole);
    Q_UNUSED(editor);
    Q_UNUSED(model);
    Q_UNUSED(index);
}

void TextSelectionDelegate::updateEditorGeometry(QWidget *editor,
                                                 const QStyleOptionViewItem &option,
                                                 const QModelIndex &/* index */) const
{
    // Set the editor's geometry to match the cell's geometry
    editor->setGeometry(option.rect);
}

// Optional: Override paint if you want custom drawing when the cell is *not* being edited
// void TextSelectionDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
//                                   const QModelIndex &index) const
// {
//     // Example: Draw standard item text, but maybe with different background/foreground
//     QStyleOptionViewItem opt = option;
//     initStyleOption(&opt, index);
//
//     // Customize options if needed (e.g., opt.backgroundBrush)
//
//     // Draw the item using the base class implementation or custom drawing
//     QStyledItemDelegate::paint(painter, opt, index);
// }
