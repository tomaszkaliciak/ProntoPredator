#include "CustomLogView.hpp"
#include "LogfileModel.hpp" // To access column enum

#include <QPainter>
#include <QScrollBar>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>
#include <QAbstractItemModel>
#include <QFontMetrics>
#include <QApplication>
#include <QTimer>
#include <QVector> // Added for QVector

CustomLogView::CustomLogView(QWidget *parent)
    : QAbstractScrollArea(parent),
      m_lastFirstVisible(-1), // Initialize last visible range trackers
      m_lastLastVisible(-1)
{
    // Use a monospaced font for predictable character widths
    m_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_font.setPointSize(12); // Match LogViewer's font size initially
    this->setFont(m_font);

    // Calculate initial metrics
    QFontMetrics fm(m_font);
    m_lineHeight = fm.height();
    // Estimate character width (might need refinement)
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    m_charWidth = fm.horizontalAdvance(QLatin1Char(' ')); // Use space width for monospaced
#else
    m_charWidth = fm.width(QLatin1Char(' '));
#endif
    if (m_charWidth <= 0) {
        m_charWidth = fm.averageCharWidth(); // Fallback
    }

    // Set up scroll bars
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    // Connect scroll bar signals to our handler slot
    // Use singleShot timer to coalesce rapid scroll events
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        // Trigger update immediately for visual feedback
        viewport()->update();
        // Use a single-shot timer to delay the cache request trigger
        QTimer::singleShot(20, this, &CustomLogView::handleScrollChange); // 20ms delay
    });
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        // Horizontal scroll only needs viewport update
        viewport()->update();
    });
    // Also connect sliderReleased to ensure final position is handled
    connect(verticalScrollBar(), &QScrollBar::sliderReleased, this, &CustomLogView::handleScrollChange);


    // Set focus policy to allow keyboard interaction later
    setFocusPolicy(Qt::StrongFocus);
}

void CustomLogView::setModel(QAbstractItemModel *model)
{
    if (m_model) {
        // Disconnect old model signals
        disconnect(m_model, &QAbstractItemModel::modelReset, this, &CustomLogView::onModelReset);
        disconnect(m_model, &QAbstractItemModel::rowsInserted, this, &CustomLogView::onRowsInserted);
        disconnect(m_model, &QAbstractItemModel::rowsRemoved, this, &CustomLogView::onRowsRemoved);
        disconnect(m_model, &QAbstractItemModel::dataChanged, this, &CustomLogView::onDataChanged);
    }

    m_model = model;

    if (m_model) {
        setupConnections();
    }

    // Reset selection and update view
    m_selection.clear();
    updateScrollBars();
    viewport()->update();
}

QAbstractItemModel *CustomLogView::model() const
{
    return m_model;
}

// Correct implementation for setHighlightRules
void CustomLogView::setHighlightRules(const QList<HighlightRule> &rules)
{
    m_highlightRules = rules;
    viewport()->update(); // Trigger repaint to apply new rules
}

void CustomLogView::setupConnections()
{
    if (!m_model) return;

    connect(m_model, &QAbstractItemModel::modelReset, this, &CustomLogView::onModelReset);
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &CustomLogView::onRowsInserted);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, &CustomLogView::onRowsRemoved);
    // Connect dataChanged to trigger viewport update and scrollbar recalculation
    connect(m_model, &QAbstractItemModel::dataChanged, this, &CustomLogView::onDataChanged);
}

// --- Event Handlers (Implementation needed) ---

void CustomLogView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event); // We'll use viewport geometry and scroll position

    if (!m_model) {
        return;
    }

    QPainter painter(viewport());
    painter.setFont(m_font);

    // Basic viewport calculations
    int firstVisibleLine = verticalScrollBar()->value() / getLineHeight();
    int lastVisibleLine = firstVisibleLine + (viewport()->height() / getLineHeight()) + 1; // +1 for partial lines
    lastVisibleLine = qMin(lastVisibleLine, m_model->rowCount() - 1);

    int lineNumAreaWidth = getLineNumberAreaWidth();
    int horizontalOffset = horizontalScrollBar()->value();

    // Background
    painter.fillRect(viewport()->rect(), viewport()->palette().base());

    // Draw visible lines
    for (int row = firstVisibleLine; row <= lastVisibleLine; ++row) {
        QModelIndex lineIndex = m_model->index(row, LogfileModel::Column::LineNumberColumn);
        QModelIndex msgIndex = m_model->index(row, LogfileModel::Column::MessageColumn);

        if (!lineIndex.isValid() || !msgIndex.isValid()) continue;

        int yPos = (row * getLineHeight()) - verticalScrollBar()->value();

        // --- Draw Line Number ---
        QString lineNumStr = m_model->data(lineIndex, Qt::DisplayRole).toString();
        // Use standard text color for better visibility against alternate base
        painter.setPen(viewport()->palette().color(QPalette::Text));
        painter.fillRect(0, yPos, lineNumAreaWidth - 5, getLineHeight(), viewport()->palette().alternateBase()); // Background for line numbers
        painter.drawText(QRect(0, yPos, lineNumAreaWidth - 5, getLineHeight()), Qt::AlignRight | Qt::AlignVCenter, lineNumStr);

        // --- Draw Message ---
        QString msgStr = m_model->data(msgIndex, Qt::DisplayRole).toString();
        // painter.setPen(viewport()->palette().color(QPalette::Active, QPalette::Text)); // Base text color set later or by formats

        QTextLayout textLayout(msgStr, m_font);

        // --- Apply Custom Highlighting Rules ---
        QVector<QTextLayout::FormatRange> formats; // Changed from QList to QVector
        for (const auto &rule : m_highlightRules) {
            if (!rule.isEnabled || rule.substring.isEmpty()) continue;

            int pos = 0;
            // TODO: Add case sensitivity option from rule?
            Qt::CaseSensitivity cs = Qt::CaseSensitive;
            while ((pos = msgStr.indexOf(rule.substring, pos, cs)) != -1) {
                QTextLayout::FormatRange range;
                range.start = pos;
                range.length = rule.substring.length();
                QTextCharFormat format;
                format.setForeground(rule.color);
                // format.setBackground(rule.color.lighter(150)); // Example: slightly lighter background
                range.format = format;
                formats.append(range);
                pos += rule.substring.length(); // Move past the found substring
            }
        }
        // TODO: Handle overlapping formats if necessary (e.g., prioritize longer matches or first rule)
        textLayout.setFormats(formats);


        textLayout.beginLayout();
        QTextLine line = textLayout.createLine();
        textLayout.endLayout();

        if (line.isValid()) {
            // --- Draw Selection Highlight ---
            if (m_selection.isValid()) {
                QModelIndex startIdx = m_selection.startLineIndex;
                QModelIndex endIdx = m_selection.endLineIndex;
                int startOffset = m_selection.startCharOffset;
                int endOffset = m_selection.endCharOffset;

                // Ensure start is before end for easier comparison
                if (startIdx.row() > endIdx.row() || (startIdx.row() == endIdx.row() && startOffset > endOffset)) {
                    qSwap(startIdx, endIdx);
                    qSwap(startOffset, endOffset);
                }

                // Check if the current row is part of the selection
                if (row >= startIdx.row() && row <= endIdx.row()) {
                    int selectionStartChar = (row == startIdx.row()) ? startOffset : 0;
                    int selectionEndChar = (row == endIdx.row()) ? endOffset : msgStr.length();
                    if (selectionEndChar > selectionStartChar) {
                        // Calculate the X coordinates for the selection rectangle
                        qreal startX = line.cursorToX(selectionStartChar);
                        qreal endX = line.cursorToX(selectionEndChar);

                        // Adjust for horizontal scroll and line number area
                        startX += lineNumAreaWidth - horizontalOffset;
                        endX += lineNumAreaWidth - horizontalOffset;

                        // Define the highlight rectangle
                        QRectF selectionRect(startX, yPos, endX - startX, getLineHeight());

                        // Clip the rectangle to the viewport bounds horizontally
                        selectionRect = selectionRect.intersected(viewport()->rect());

                        // Draw the highlight
                        painter.fillRect(selectionRect, viewport()->palette().highlight());

                        // Set text color for selected text (optional, depends on highlight color)
                        // painter.setPen(viewport()->palette().color(QPalette::HighlightedText));
                    }
                }
            }

            // --- Draw Text ---
            // Reset pen color in case selection changed it (might not be needed with formats)
            // painter.setPen(viewport()->palette().color(QPalette::Active, QPalette::Text)); // Base text color is handled by layout/formats

            // Draw text considering horizontal scroll offset
            // Selection background is drawn above, text color within selection might be overridden by formats.
            line.draw(&painter, QPoint(lineNumAreaWidth - horizontalOffset, yPos));
        }
    }

    // Selection highlight is now drawn per line
}

void CustomLogView::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    updateScrollBars();
    viewport()->update(); // Redraw needed if viewport size changes
}

void CustomLogView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_model) {
        m_dragStartPosition = event->pos();
        m_isSelecting = true;
        m_selection.clear();

        int charOffset = -1;
        QModelIndex startIndex = indexAtPosition(event->pos(), &charOffset);

        if (startIndex.isValid() && charOffset != -1) {
            m_selection.startLineIndex = startIndex;
            m_selection.startCharOffset = charOffset;
            // Initialize end to the same position initially
            m_selection.endLineIndex = startIndex;
            m_selection.endCharOffset = charOffset;
        }
        viewport()->update(); // Redraw to clear previous selection
        event->accept();
    } else {
        QAbstractScrollArea::mousePressEvent(event);
    }
}

void CustomLogView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isSelecting && (event->buttons() & Qt::LeftButton) && m_model) {
        int charOffset = -1;
        QModelIndex currentIndex = indexAtPosition(event->pos(), &charOffset);

        if (currentIndex.isValid() && charOffset != -1) {
            // Update the end of the selection
            m_selection.endLineIndex = currentIndex;
            m_selection.endCharOffset = charOffset;

            // TODO: Implement auto-scrolling if near edge
            // ensureIndexVisible(currentIndex); // Basic visibility check

            viewport()->update(); // Redraw to show selection extending
        }
        event->accept();
    } else {
         QAbstractScrollArea::mouseMoveEvent(event);
    }
}

void CustomLogView::mouseReleaseEvent(QMouseEvent *event)
{
     if (event->button() == Qt::LeftButton && m_isSelecting) {
        m_isSelecting = false;
        // Final selection is already set by mouseMoveEvent
        // Optional: Optimize selection range (ensure start <= end)
        // Optional: Emit a signal selectionChanged()
        viewport()->update(); // Final redraw of selection
        event->accept();
    } else {
        QAbstractScrollArea::mouseReleaseEvent(event);
    }
}

void CustomLogView::wheelEvent(QWheelEvent *event)
{
    if (!m_model || m_lineHeight <= 0) {
        event->ignore(); // Ignore if no model or line height is invalid
        return;
    }

    QScrollBar *vBar = verticalScrollBar();
    int numDegrees = event->angleDelta().y() / 8;
    int numSteps = numDegrees / 15; // Standard step calculation

    if (numSteps != 0) {
        // Scroll by 3 lines per standard step
        int scrollAmount = numSteps * 3 * m_lineHeight;
        vBar->setValue(vBar->value() - scrollAmount);
        event->accept();
    } else {
        event->ignore(); // Ignore if no significant scroll delta
    }
}

// --- Helper Methods (Implementation needed/refined) ---

int CustomLogView::getLineHeight() const
{
    return m_lineHeight > 0 ? m_lineHeight : 15; // Provide a default if not calculated
}

int CustomLogView::getLineNumberAreaWidth() const
{
    // Calculate based on max possible line number digits + padding
    int maxDigits = m_model ? QString::number(m_model->rowCount()).length() : 5; // Estimate 5 digits if no model
    maxDigits = qMax(maxDigits, 4); // Minimum width for ~4 digits
    QFontMetrics fm(m_font);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    int width = fm.horizontalAdvance(QString(maxDigits, QLatin1Char('9'))) + 10; // Width of '9' * digits + padding
#else
    int width = fm.width(QString(maxDigits, QLatin1Char('9'))) + 10;
#endif
    return width;
}

int CustomLogView::getTotalContentHeight() const
{
    return m_model ? m_model->rowCount() * getLineHeight() : 0;
}

int CustomLogView::getTotalContentWidth() const
{
    // TODO: Needs refinement for accurate horizontal scrolling.
    // Calculating the true maximum width requires iterating through the entire model,
    // which is too slow for large files or frequent updates.
    // Using a fixed estimate or width of visible lines is a compromise.
    // For now, estimate based on viewport width.
    int estimatedCharWidth = m_charWidth > 0 ? m_charWidth : 8; // Use calculated or default char width
    int estimatedChars = 200; // Assume a max line length of 200 chars for estimation
    int estimatedWidth = getLineNumberAreaWidth() + (estimatedChars * estimatedCharWidth);
    // Ensure it's at least the viewport width
    return qMax(estimatedWidth, viewport()->width() + horizontalScrollBar()->value());
}

QModelIndex CustomLogView::indexAtPosition(const QPoint &position, int *charOffset) const
{
    if (!m_model || m_lineHeight <= 0) {
        return QModelIndex();
    }

    // Calculate row
    int row = verticalScrollBar()->value() / m_lineHeight + position.y() / m_lineHeight;
    if (row < 0 || row >= m_model->rowCount()) {
        return QModelIndex();
    }

    // Calculate character offset (simplified)
    int lineNumAreaWidth = getLineNumberAreaWidth();
    int xInMessage = position.x() - lineNumAreaWidth + horizontalScrollBar()->value();

    if (charOffset) {
        if (xInMessage < 0) {
            *charOffset = 0; // Clicked in line number area or before message start
        } else {
            // Use QTextLayout to find character index for the position
            QModelIndex msgIndex = m_model->index(row, LogfileModel::Column::MessageColumn);
            QString msgStr = m_model->data(msgIndex, Qt::DisplayRole).toString();
            QTextLayout textLayout(msgStr, m_font);
            textLayout.beginLayout();
            QTextLine line = textLayout.createLine();
            textLayout.endLayout();

            if (line.isValid()) {
                *charOffset = line.xToCursor(xInMessage);
            } else {
                *charOffset = 0; // Default to start if layout fails
            }
             // Clamp to valid range
            *charOffset = qBound(0, *charOffset, msgStr.length());
        }
    }

    // Return index for the message column
    return m_model->index(row, LogfileModel::Column::MessageColumn);
}


void CustomLogView::ensureIndexVisible(const QModelIndex &index)
{
    if (!index.isValid() || !m_model || m_lineHeight <= 0) return;

    int row = index.row();
    int yPos = row * m_lineHeight;

    // Vertical scroll adjustment
    int currentV = verticalScrollBar()->value();
    int viewportH = viewport()->height();

    if (yPos < currentV) {
        // Scroll up
        verticalScrollBar()->setValue(yPos);
    } else if (yPos + m_lineHeight > currentV + viewportH) {
        // Scroll down
        verticalScrollBar()->setValue(yPos + m_lineHeight - viewportH);
    }

    // Horizontal scroll adjustment (basic - ensure start is visible)
    // TODO: Refine horizontal scrolling based on character offset if needed
    // int currentH = horizontalScrollBar()->value();
    // int viewportW = viewport()->width();
    // int lineNumAreaW = getLineNumberAreaWidth();
    // Need character position X to scroll horizontally correctly.
}


// --- Slots for Model Changes ---

void CustomLogView::updateScrollBars()
{
    int contentHeight = getTotalContentHeight();
    int contentWidth = getTotalContentWidth(); // Needs refinement
    int viewportHeight = viewport()->height();
    int viewportWidth = viewport()->width();

    verticalScrollBar()->setPageStep(viewportHeight);
    verticalScrollBar()->setRange(0, qMax(0, contentHeight - viewportHeight));

    horizontalScrollBar()->setPageStep(viewportWidth);
    horizontalScrollBar()->setRange(0, qMax(0, contentWidth - viewportWidth));
}

void CustomLogView::onModelReset()
{
    m_selection.clear();
    updateScrollBars();
    viewport()->update();
}

void CustomLogView::onRowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    // Could potentially optimize repaint area, but full update is safer for now
    updateScrollBars();
    viewport()->update();
}

void CustomLogView::onRowsRemoved(const QModelIndex &parent, int first, int last)
{
     Q_UNUSED(parent);
     Q_UNUSED(first);
     Q_UNUSED(last);
     // Selection might become invalid, clear it for safety
     m_selection.clear();
     updateScrollBars();
     viewport()->update();
}

void CustomLogView::onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    Q_UNUSED(roles);
    // Could potentially optimize repaint area based on topLeft/bottomRight
    // For now, assume any data change might affect layout/width
    updateScrollBars(); // Width might change
    viewport()->update();
}

// --- Selection Retrieval (Implementation needed) ---

QString CustomLogView::getSelectedText() const
{
    if (!m_selection.isValid() || !m_model) {
        return QString();
    }

    QString selectedText;
    QModelIndex startIdx = m_selection.startLineIndex;
    QModelIndex endIdx = m_selection.endLineIndex;
    int startOffset = m_selection.startCharOffset;
    int endOffset = m_selection.endCharOffset;

    // Ensure start is before end (row-major, then column/offset)
    if (startIdx.row() > endIdx.row() || (startIdx.row() == endIdx.row() && startOffset > endOffset)) {
        qSwap(startIdx, endIdx);
        qSwap(startOffset, endOffset);
    }

    for (int row = startIdx.row(); row <= endIdx.row(); ++row) {
        QModelIndex msgIndex = m_model->index(row, LogfileModel::Column::MessageColumn);
        if (!msgIndex.isValid()) continue;

        QString lineText = m_model->data(msgIndex, Qt::DisplayRole).toString();
        int lineLen = lineText.length();

        int selectionStart = (row == startIdx.row()) ? startOffset : 0;
        int selectionEnd = (row == endIdx.row()) ? endOffset : lineLen;

        if (selectionStart < lineLen && selectionEnd > selectionStart) {
             selectedText += lineText.mid(selectionStart, selectionEnd - selectionStart);
        }

        // Add newline between lines in multi-line selection
        if (row < endIdx.row()) {
            selectedText += '\n';
        }
    }

    return selectedText;
}

// Private slot to handle scroll changes and emit visibleRangeChanged
void CustomLogView::handleScrollChange()
{
    if (!m_model || m_lineHeight <= 0) return;

    int firstVisible = verticalScrollBar()->value() / m_lineHeight;
    int viewportLines = viewport()->height() / m_lineHeight;
    int lastVisible = firstVisible + viewportLines;
    lastVisible = qMin(lastVisible, m_model->rowCount() - 1); // Clamp to model size

    // Check if the range has changed significantly (e.g., by half a page)
    // or if it's the first time
    int threshold = viewportLines / 2;
    if (m_lastFirstVisible == -1 || qAbs(firstVisible - m_lastFirstVisible) >= threshold)
    {
        m_lastFirstVisible = firstVisible;
        m_lastLastVisible = lastVisible;
        // Emit the signal with 1-based line numbers for Logfile
        emit visibleRangeChanged(firstVisible + 1, lastVisible + 1);
        // qDebug() << "Emitted visibleRangeChanged:" << firstVisible + 1 << "-" << lastVisible + 1;
    }
}


#include <QSortFilterProxyModel> // Needed for casting check

// Method to get the source model index corresponding to the start of the current selection
QModelIndex CustomLogView::getSelectedSourceIndex() const
{
    if (!m_selection.isValid() || !m_model) {
        return QModelIndex(); // No valid selection or model
    }

    // The selection stores the index relative to the *current* model (which might be a proxy)
    QModelIndex proxyIndex = m_selection.startLineIndex; // Use the start of the selection

    // Try to cast the model to a proxy model to map back to source
    QSortFilterProxyModel* proxyModel = qobject_cast<QSortFilterProxyModel*>(m_model);

    if (proxyModel) {
        // Ensure the proxy index is still valid before mapping
        if (!proxyIndex.isValid() || proxyIndex.model() != proxyModel) {
             qWarning("getSelectedSourceIndex: Stored proxy index is invalid or belongs to a different model.");
             return QModelIndex();
        }
        return proxyModel->mapToSource(proxyIndex);
    } else {
        // If it's not a proxy model, the index is already the source index
        // Ensure the index is still valid
         if (!proxyIndex.isValid() || proxyIndex.model() != m_model) {
             qWarning("getSelectedSourceIndex: Stored source index is invalid or belongs to a different model.");
             return QModelIndex();
         }
        return proxyIndex;
    }
}
