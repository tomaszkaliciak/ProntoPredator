#include "LogViewer.hpp"
#include "CustomLogView.hpp" // Added include for the new view

#include <QVBoxLayout> // Changed from QGridLayout
#include <QDebug>
#include <QMessageBox>
#include <QAbstractItemModel>
#include <QWidget> // Added for viewport()->setTextInteractionFlags
#include <QStackedLayout>
#include <QProgressDialog> // Added include
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

// Constructor for single view setup with progress dialog
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

    // Set the main layout for the LogViewer widget
    QVBoxLayout* mainLayout = new QVBoxLayout(this); // Keep QVBoxLayout
    mainLayout->addWidget(view_); // Add view directly
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Connect signals from proxy model
    connect(proxyModel_, &EfficientLogFilterProxyModel::filteringStarted, this, &LogViewer::onFilteringStarted); // Changed type
    connect(proxyModel_, &EfficientLogFilterProxyModel::filteringFinished, this, &LogViewer::onFilteringFinished); // Changed type

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
    if (!proxyModel_) return;

    // Pass the chain to the proxy model
    proxyModel_->applyFilterChain(chain);
}

// --- Slots for Async Filtering ---

void LogViewer::onFilteringStarted()
{
    qDebug() << "LogViewer: Filtering started...";
    view_->setEnabled(false); // Disable view interaction during filtering

    // Create and show progress dialog
    if (!filterProgressDialog_) { // Create only if it doesn't exist
        filterProgressDialog_ = new QProgressDialog("Filtering log...", "Cancel", 0, 0, this); // Indeterminate progress
        filterProgressDialog_->setWindowModality(Qt::WindowModal);
        filterProgressDialog_->setAutoClose(false); // We will close it manually
        filterProgressDialog_->setAutoReset(false); // We will reset manually if needed
        // Connect cancel button to proxy model's cancel slot
        connect(filterProgressDialog_, &QProgressDialog::canceled, proxyModel_, &EfficientLogFilterProxyModel::cancelFiltering); // Changed type
        filterProgressDialog_->show();
    }
}

void LogViewer::onFilteringFinished(int matchCount)
{
     qDebug() << "LogViewer: Filtering finished. Matches:" << matchCount;
     view_->setEnabled(true); // Re-enable view interaction

     // Close and delete the progress dialog
     if (filterProgressDialog_) {
         filterProgressDialog_->close();
         filterProgressDialog_->deleteLater();
         filterProgressDialog_ = nullptr;
     }
     // Optionally update status bar or other UI elements here
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
