#include "TextRenderer.hpp"

#include <QColor>
#include <QList>
#include <QPlainTextEdit>
#include <QPainter>
#include <QTextBlock>

#include "LineNumberArea.hpp"
#include "Logfile.hpp" // Include Logfile header for Logfile definition

// Removed linesToQString function as it's no longer applicable

TextRenderer::TextRenderer(QWidget* parent, Logfile* logfile)
    : QPlainTextEdit(parent), logfile_(logfile) // Initialize logfile_ member
{
    // lineNumberArea = new LineNumberArea(this); // Assuming LineNumberArea is defined elsewhere
    // If LineNumberArea is defined in its own header, include that.
    // For now, assume it's available. Let's use the specific type from the header.
    lineNumberArea = new LineNumberArea(this);


    connect(this, &TextRenderer::blockCountChanged, this, &TextRenderer::updateLineNumberAreaWidth);
    // The updateRequest signal signature might need adjustment if QPlainTextEdit version differs
    // connect(this, &TextRenderer::updateRequest, this, &TextRenderer::updateLineNumberArea);
    // Let's use the correct signature based on typical QPlainTextEdit usage
     connect(this, &QPlainTextEdit::updateRequest, this, &TextRenderer::updateLineNumberArea);


    connect(this, &TextRenderer::cursorPositionChanged, this, &TextRenderer::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
    setFont(QFont(QString("Courier New")));
    setReadOnly(true); // Keep read-only for now
    setWordWrapMode(QTextOption::NoWrap); // Keep no-wrap

    // DO NOT set plain text here. QPlainTextEdit cannot handle huge files.
    // The actual display needs to be driven by the model/view framework
    // or a custom implementation that loads text on demand.
    // setPlainText(linesToQString(content)); // REMOVED

    // We need to somehow tell QPlainTextEdit the total number of lines
    // so its scrollbar behaves correctly. This is tricky without loading
    // content. A common workaround is to insert placeholder lines,
    // but that also consumes memory.
    // For now, the scrollbar might not reflect the true file size.
    // A better solution requires replacing QPlainTextEdit.
    if (logfile_) {
        // Maybe set block count hint? (This is not standard API)
        // Or insert dummy lines (inefficient):
        // this->document()->reset(); // Clear existing content
        // this->document()->setPlainText(QString(logfile_->getLineCount(), '\n'));
        // The above is still bad. Let's leave it unpopulated for now.
    }
}

void TextRenderer::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;
    QTextEdit::ExtraSelection selection;
    QColor lineColor = QColor(Qt::yellow).lighter(160);
    selection.format.setBackground(lineColor);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    extraSelections.append(selection);
    setExtraSelections(extraSelections);
}

// Calculate width needed for line numbers based on total line count from Logfile
int TextRenderer::lineNumberAreaWidth()
{
    int digits = 1;
    qint64 max_lines = 1; // Default to 1

    if (logfile_) {
        max_lines = qMax(1LL, logfile_->getLineCount()); // Use 1LL for qint64 literal
    }

    // Calculate number of digits required for the max line number
    qint64 temp = max_lines;
    while (temp >= 10) {
        temp /= 10;
        ++digits;
    }

    // Add some padding

    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;

    return space;
}

void TextRenderer::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void TextRenderer::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy) lineNumberArea->scroll(0, dy);
    else lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect())) updateLineNumberAreaWidth(0);
}

void TextRenderer::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);
    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void TextRenderer::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    // Check if logfile is valid
    if (!logfile_ || logfile_->getLineCount() == 0) {
        // Optionally paint a default state or just return
        QPainter painter(lineNumberArea);
        painter.fillRect(event->rect(), QColor(Qt::lightGray)); // Indicate disabled/empty state
        return;
    }

    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(),  QColor(Qt::gray).lighter(150)); // Original background
    QTextBlock block = firstVisibleBlock();
    // Get the bounding geometry and offset like the original code
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    // Iterate through visible blocks
    while (block.isValid() && top <= event->rect().bottom()) // Keep iterating as long as block is valid and within paint area
    {
        // Check if the block is actually visible within the viewport's bounds
        if (block.isVisible() && bottom >= event->rect().top())
        {
            // Get the line number: block numbers are 0-based, log lines are 1-based
            qint64 current_line_number = block.blockNumber() + 1;

            // Check if this line number is valid within the logfile
            if (current_line_number <= logfile_->getLineCount()) {
                 QString number = QString::number(current_line_number); // Use block number + 1
                 painter.setFont(QFont(QString("Courier New"))); // Use consistent font
                 painter.setPen(Qt::black);
                 painter.drawText(0, top, lineNumberArea->width() - 3, fontMetrics().height(), // Add small right margin
                     Qt::AlignRight, number);
            }
            // Else: Block number exceeds actual lines, don't draw number (can happen if placeholder lines were added)
        }

        block = block.next();
        // Recalculate top/bottom for the next block BEFORE checking loop condition again
        // Ensure blockBoundingRect is valid before using it
        if (!block.isValid()) break; // Exit if next block is invalid
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        // Add safety break for zero-height blocks to prevent infinite loop
        if (top == bottom && block.isValid()) break;
    }
}
