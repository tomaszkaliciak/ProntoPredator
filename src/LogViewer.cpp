#include "LogViewer.hpp"

#include <QTableView>
#include <QHeaderView>
#include <QGridLayout>
#include <QDebug>
#include <QMessageBox>
#include <QAbstractItemModel>
// #include <QLabel> // Removed overlay label include
#include <QStackedLayout>
#include <QProgressDialog> // Added include

#include "Logfile.hpp"
#include "LogfileModel.hpp"
#include "LogFilterProxyModel.hpp"
#include "GrepNode.hpp"

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
    proxyModel_ = new LogFilterProxyModel(this);
    proxyModel_->setSourceModel(baseSourceModel_);
    proxyModel_->setSourceLogfile(logfile_);

    // Create the Table View
    view_ = new QTableView(this);
    view_->setModel(proxyModel_);

    // Configure the Table View
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    view_->setWordWrap(false);
    view_->setShowGrid(false);
    view_->verticalHeader()->setVisible(true);
    view_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    view_->horizontalHeader()->setVisible(true);
    view_->horizontalHeader()->setStretchLastSection(true);
    view_->setColumnWidth(LogfileModel::Column::LineNumberColumn, 80);
    view_->setStyleSheet(
        "QTableView::item:selected { background-color: #FFFFE0; color: black; }"
    );

    // Set the main layout for the LogViewer widget
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(view_); // Add view directly
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Connect signals from proxy model
    connect(proxyModel_, &LogFilterProxyModel::filteringStarted, this, &LogViewer::onFilteringStarted);
    connect(proxyModel_, &LogFilterProxyModel::filteringFinished, this, &LogViewer::onFilteringFinished);

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
        connect(filterProgressDialog_, &QProgressDialog::canceled, proxyModel_, &LogFilterProxyModel::cancelFiltering);
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


// --- Accessors ---

Logfile* LogViewer::getLogfile()
{
    return logfile_;
}

LogfileModel* LogViewer::getBaseSourceModel() const
{
    return baseSourceModel_;
}

QTableView* LogViewer::getTableView() const
{
    return view_;
}
