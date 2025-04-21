#include "LogfileModel.hpp"

#include <QVariant>
#include <QFont> // Include QFont for setting monospace font
#include <QDebug> // For potential debugging

LogfileModel::LogfileModel(Logfile* logfile, QObject* parent)
    : QAbstractListModel(parent), logfile_(logfile)
{
    if (!logfile_) {
        qWarning("LogfileModel created with a null Logfile pointer!");
        // Consider throwing an exception or handling this error appropriately
    }
    // Optional: Connect signals from logfile_ if it can change dynamically
    // connect(logfile_, &Logfile::dataChangedSignal, this, &LogfileModel::handleDataChange);
}

int LogfileModel::rowCount(const QModelIndex &parent) const
{
    // If the parent is valid, it means we're asking for children of an item,
    // which doesn't apply to a flat list/table model.
    if (parent.isValid() || !logfile_) {
        return 0;
    }

    // Return the total number of lines in the log file
    return static_cast<int>(logfile_->getLineCount()); // Cast qint64 to int
}

// --- New/Modified for TableView ---

int LogfileModel::columnCount(const QModelIndex &parent) const
{
    // We have two columns: Line Number and Message
    return parent.isValid() ? 0 : 2;
}

QVariant LogfileModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case Column::LineNumberColumn:
                return tr("Line");
            case Column::MessageColumn:
                return tr("Message");
            default:
                return QVariant();
        }
    }
    // Optional: Provide vertical header data (row numbers) if needed,
    // but QTableView can often show them automatically.
    // if (orientation == Qt::Vertical) {
    //     return section + 1; // 1-based row numbers
    // }
    return QVariant();
}

QVariant LogfileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !logfile_) {
        return QVariant();
    }

    // Adjust row index to be 1-based for Logfile::getLine
    qint64 line_number = static_cast<qint64>(index.row()) + 1;

    // Check if the line number is within the valid range
    if (line_number < 1 || line_number > logfile_->getLineCount()) {
        return QVariant();
    }

    // --- Handle roles based on column ---
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case Column::LineNumberColumn:
                return QVariant::fromValue(line_number); // Return the line number itself
            case Column::MessageColumn:
                // Fetch the specific line from the Logfile object only for the message column
                return logfile_->getLine(line_number).text;
            default:
                return QVariant();
        }
    }
    // Handle the font role (set a monospace font for the message column)
    else if (role == Qt::FontRole) {
         if (index.column() == Column::MessageColumn) {
             return QFont("Courier New"); // Or another suitable monospace font
         }
         // Optionally set a different font for the line number column
    }
    // Optional: Handle alignment role for line numbers
    else if (role == Qt::TextAlignmentRole) {
        if (index.column() == Column::LineNumberColumn) {
            // Cast alignment flags to int for QVariant
            return QVariant(static_cast<int>(Qt::AlignRight | Qt::AlignVCenter));
        }
    }


    // Return an invalid QVariant for unhandled roles/columns
    return QVariant();
}

// Optional: Implement handleDataChange if Logfile can be modified externally
// void LogfileModel::handleDataChange() {
//     beginResetModel(); // Or more specific signals like dataChanged, rowsInserted, etc.
//     // Update internal state if necessary
//     endResetModel();
// }
