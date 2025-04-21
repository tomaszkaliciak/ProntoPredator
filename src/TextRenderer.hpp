#ifndef TEXT_RENDERER_HPP
#define TEXT_RENDERER_HPP

#include <memory> // Keep for potential future use

#include <QPlainTextEdit>
// #include "Logfile.hpp" // Forward declare
class Logfile; // Forward declaration

// #include "ILineNumberingPolicy.hpp" // Removed

class LineNumberArea; // Forward declaration if needed

class TextRenderer : public QPlainTextEdit
{
    Q_OBJECT // Add Q_OBJECT macro for signals/slots

public:
    // Constructor now takes Logfile pointer
    // Remove ILineNumberingPolicy as QPlainTextEdit handles numbering,
    // and the custom policy was likely tied to the old Lines vector.
    TextRenderer(QWidget* parent, Logfile* logfile);

    // Line number area methods remain, but implementation will change
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    // QWidget *lineNumberArea; // Keep - this is the custom line number display
    LineNumberArea *lineNumberArea; // Use specific type if available
    Logfile* logfile_; // Store pointer to the logfile data source
};

#endif  // TEXT_RENDERER_HPP
