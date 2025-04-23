#include "LogViewer.hpp"
#include "CustomLogView.hpp" // Added include for the new view

#include <QVBoxLayout> // Changed from QGridLayout
#include <QDebug>
#include <QMessageBox>
#include <QAbstractItemModel>
#include <QWidget> // Added for viewport()->setTextInteractionFlags
#include <QLabel> // Added include
// #include <QProgressDialog> // Removed include
#include <QAction> // For Copy action
#include <QKeySequence> // For standard shortcuts
#include <QClipboard> // For clipboard access
#include <QApplication> // For clipboard access
#include <QStringList> // For joining lines (still needed for applyFilterChain?) - Keep for now

#include "Logfile.hpp"
#include "LogfileModel.hpp"
#include "EfficientLogFilterProxyModel.hpp" // Changed include
#include "GrepNode.hpp"
// #include "TextSelectionDelegate.hpp" // No longer needed here

// Constructor for single view setup with status label
LogViewer::LogViewer(QWidget* parent, Logfile* logfile)
    : QWidget(parent),
      logfile_(logfile)
{
    if (!logfile_) {
        qWarning("LogViewer created with a null Logfile pointer!");
        return;
    }

    // Create the base source model
    baseSourceModel_ = new LogfileModel(logfile_, this);

    // Create the proxy model
    proxyModel_ = new EfficientLogFilterProxyModel(this); // Changed type
    proxyModel_->setSourceModel(baseSourceModel_);
    proxyModel_->setSourceLogfile(logfile_); // Keep this for the proxy model

    // Create the Custom View
    view_ = new CustomLogView(this);
    view_->setModel(proxyModel_); // Set the proxy model on the custom view

    // Font setting is handled within CustomLogView constructor, but ensure consistency if needed
    // QFont font = view_->font();
    // font.setPointSize(12);
    // view_->setFont(font);

    // Remove QTableView specific configurations:
    // - Selection behavior/mode (handled by CustomLogView mouse events)
    // - Edit triggers (not applicable in the same way)
    // - Scroll modes (handled by QAbstractScrollArea base)
    // - Word wrap (CustomLogView currently doesn't wrap)
    // - Grid, headers, column widths (handled by CustomLogView paintEvent)
    // - Stylesheet for selection (handled by CustomLogView paintEvent)
    // - Item delegate (CustomLogView handles drawing and selection directly)

    // Create Status Label
    statusLabel_ = new QLabel(tr("Ready"), this); // Initial text
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setVisible(false); // Initially hidden
    statusLabel_->setStyleSheet("QLabel { background-color: yellow; padding: 2px; }"); // Basic styling

    // Set the main layout for the LogViewer widget
    QVBoxLayout* mainLayout = new QVBoxLayout(this); // Keep QVBoxLayout
    mainLayout->addWidget(view_); // Add view
    mainLayout->addWidget(statusLabel_); // Add status label below view
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0); // No space between view and label

    // Connect signals from proxy model
    connect(proxyModel_, &EfficientLogFilterProxyModel::filteringStarted, this, &LogViewer::onFilteringStarted);
    connect(proxyModel_, &EfficientLogFilterProxyModel::filteringFinished, this, &LogViewer::onFilteringFinished);

    // --- Add Copy Action ---
    QAction* copyAction = new QAction(tr("Copy"), this);
    copyAction->setShortcut(QKeySequence::Copy); // Standard Ctrl+C / Cmd+C
    connect(copyAction, &QAction::triggered, this, &LogViewer::copySelectionToClipboard);
    this->addAction(copyAction); // Add action to the LogViewer widget itself

    // Optional: Add to context menu (if desired)
    // view_->setContextMenuPolicy(Qt::ActionsContextMenu);
    // view_->addAction(copyAction);

#include <QList> // For QList parameter

}

// Method to apply filtering criteria using a chain of nodes
void LogViewer::applyFilterChain(const QList<GrepNode*>& chain)
{
    qDebug() << "LogViewer::applyFilterChain: Received chain of size:" << chain.size();
    if (!proxyModel_) {
        qWarning("LogViewer::applyFilterChain: Proxy model is null!");
        return;
    }

    // Pass the chain to the proxy model
    proxyModel_->applyFilterChain(chain);
}

// --- Slots for Async Filtering ---

void LogViewer::onFilteringStarted()
{
    qDebug() << "LogViewer: Filtering started...";
    view_->setEnabled(false); // Disable view interaction during filtering

    // Show status label instead of dialog
    if (statusLabel_) {
        statusLabel_->setText(tr("Filtering..."));
        statusLabel_->setVisible(true);
    }
}

void LogViewer::onFilteringFinished(int matchCount)
{
     qDebug() << "LogViewer: Filtering finished. Matches:" << matchCount;
     view_->setEnabled(true); // Re-enable view interaction

     // Update status label and hide it after a short delay
     if (statusLabel_) {
         statusLabel_->setText(tr("Filtering finished. Matches: %1").arg(matchCount));
         // Optionally hide the label after a delay
         QTimer::singleShot(2000, statusLabel_, &QLabel::hide); // Hide after 2 seconds
     }
}

// --- Slot for Copying ---

void LogViewer::copySelectionToClipboard()
{
    if (!view_) return;

    QString selectedText = view_->getSelectedText(); // Use the new method

    if (!selectedText.isEmpty()) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(selectedText);
        qDebug() << "Copied selected text to clipboard.";
        // Optionally count lines if needed: qDebug() << "Copied" << selectedText.count('\n') + 1 << "lines (approx) to clipboard.";
    }
}


// --- Accessors ---

Logfile* LogViewer::getLogfile()
{
    return logfile_;
}

LogfileModel* LogViewer::getBaseSourceModel() const
{
    return baseSourceModel_;
}

// Updated accessor
CustomLogView* LogViewer::getCustomView() const
{
    return view_;
}
