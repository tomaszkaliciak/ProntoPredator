#ifndef CUSTOM_LOG_VIEW_HPP
#define CUSTOM_LOG_VIEW_HPP

#include <QAbstractScrollArea>
#include <QPoint>
#include <QTextLayout> // For text layout and selection
#include <QPersistentModelIndex> // Added for QPersistentModelIndex

// Forward declarations
class LogfileModel;
class EfficientLogFilterProxyModel;
class QAbstractItemModel;

// Structure to represent a selection range (start/end character index within the *entire* log content)
// We might need a more sophisticated representation later, considering virtual scrolling.
// For now, let's think in terms of model indices and character offsets within a line.
struct TextSelection {
    QPersistentModelIndex startLineIndex;
    int startCharOffset = -1; // Character offset within the start line's message
    QPersistentModelIndex endLineIndex;
    int endCharOffset = -1;   // Character offset within the end line's message

    bool isValid() const {
        return startLineIndex.isValid() && endLineIndex.isValid() && startCharOffset >= 0 && endCharOffset >= 0;
    }
    void clear() {
        startLineIndex = QPersistentModelIndex();
        startCharOffset = -1;
        endLineIndex = QPersistentModelIndex();
        endCharOffset = -1;
    }
};


class CustomLogView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    explicit CustomLogView(QWidget *parent = nullptr);

    void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const;

    // Method to get the currently selected text
    QString getSelectedText() const;

    // Method to scroll to ensure a specific model index is visible
    void ensureIndexVisible(const QModelIndex &index); // Make public

    // Method to get the source model index corresponding to the start of the current selection
    QModelIndex getSelectedSourceIndex() const;

signals:
    // Emitted when the range of visible lines changes significantly (e.g., due to scrolling)
    void visibleRangeChanged(qint64 firstVisible, qint64 lastVisible);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    // void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateScrollBars();
    void onModelReset();
    void handleScrollChange(); // Declare the slot
    void onRowsInserted(const QModelIndex &parent, int first, int last);
    void onRowsRemoved(const QModelIndex &parent, int first, int last);
    void onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles = QVector<int>());

private:
    void setupConnections();
    int getLineHeight() const;
    int getLineNumberAreaWidth() const;
    int getTotalContentHeight() const;
    int getTotalContentWidth() const; // May need refinement for long lines
    QModelIndex indexAtPosition(const QPoint &position, int *charOffset = nullptr) const; // Get model index and char offset at a viewport position
    // ensureIndexVisible moved to public section

    QAbstractItemModel *m_model = nullptr;
    QFont m_font;
    int m_charWidth = 0; // Average char width for estimations
    int m_lineHeight = 0;

    // Selection state
    TextSelection m_selection;
    QPoint m_dragStartPosition;
    bool m_isSelecting = false;

    // Track last emitted visible range to avoid excessive signals
    qint64 m_lastFirstVisible = -1;
    qint64 m_lastLastVisible = -1;

};

#endif // CUSTOM_LOG_VIEW_HPP
