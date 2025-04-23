#include "LoaderLogFile.hpp"

// Use qualified includes
#include <QtCore/QRegularExpression>
#include <QtWidgets/QTabWidget> // Assuming QTabWidget is in QtWidgets
#include <QtCore/QString>
#include <QtCore/QDebug> // For qWarning
#include <QtCore/QFileInfo> // Added for filename extraction
#include <QtCore/QPointer> // Added for safe pointers in lambdas

// #include "../BookmarksModel.hpp" // Unused include
#include "../GrepNode.hpp"
#include "../Logfile.hpp" // Add include for full Logfile definition
#include "../LogfileModel.hpp" // Added for model reset

#include "../LogViewer.hpp"
#include "../FileViewer.hpp"
// #include "ui_MainWindow.h" // Removed

namespace loader
{

// Update function signature to match header
void LogfileLoader::load(QTabWidget *target_tab_widget, ::Logfile* logfile_data, std::function<void(FileViewer*)> connect_slots_to_manager) // Renamed class
{
    // Ensure logfile_data is valid
    if (!logfile_data) {
        qWarning("LogfileLoader::load called with null Logfile pointer."); // Updated warning message
        return;
    }
    // Ensure target_tab_widget is valid
    if (!target_tab_widget) {
         qWarning("LogfileLoader::load called with null target_tab_widget pointer."); // Updated warning message
         return;
    }

    // Pass target_tab_widget as parent for memory management
    // The FileViewer constructor likely creates the LogfileModel internally.
    FileViewer* viewer = new FileViewer(target_tab_widget, logfile_data); // logfile_data is not yet initialized!
    connect_slots_to_manager(viewer); // Connect manager slots (e.g., for closing)

    // Get filename for display (might be empty initially if Logfile constructor changed)
    // It's better to get it after initialization or use a placeholder.
    QString short_filename = QFileInfo(logfile_data->getFileName()).fileName();
    if (short_filename.isEmpty()) {
        short_filename = "Loading...";
    }

    // Add the tab, initially showing "Loading..."
    int tab_index = target_tab_widget->addTab(viewer, short_filename);
    target_tab_widget->setTabToolTip(tab_index, logfile_data->getFileName()); // Tooltip can show full path

    // Initially disable the viewer until loading is complete
    viewer->setEnabled(false);

    // --- Connect to Logfile signals ---
    // Use QPointer for safety in lambdas, although viewer should outlive logfile_data here
    QPointer<FileViewer> viewer_ptr = viewer;
    QPointer<QTabWidget> tab_widget_ptr = target_tab_widget;
    QPointer<::Logfile> logfile_ptr = logfile_data; // Use QPointer for logfile too

    // Connect indexing finished signal
    QObject::connect(logfile_data, &::Logfile::indexingFinished, viewer, // Context object for connection lifetime
        [viewer_ptr, tab_widget_ptr, logfile_ptr, tab_index](bool success) {
        if (!viewer_ptr || !tab_widget_ptr || !logfile_ptr) return; // Check pointers

        if (success) {
            qInfo() << "Indexing finished for" << logfile_ptr->getFileName() << ", resetting model.";
            // Reset the model now that data is available
            // Access the model via FileViewer -> LogViewer -> LogfileModel
            if (viewer_ptr->getLogViewer()) { // Check if LogViewer exists
                if (auto* model = viewer_ptr->getLogViewer()->getBaseSourceModel()) {
                    // Call the public reset method
                    model->resetModel();
                } else {
                    qWarning("Could not get baseSourceModel from LogViewer to reset.");
                }
            } else {
                 qWarning("Could not get LogfileModel from FileViewer to reset.");
            }

            // Update tab title and tooltip now that filename is definitely set
            QString final_filename = logfile_ptr->getFileName();
            tab_widget_ptr->setTabText(tab_index, QFileInfo(final_filename).fileName());
            tab_widget_ptr->setTabToolTip(tab_index, final_filename);
            // Optionally re-enable UI elements in the viewer if they were disabled during loading
            viewer_ptr->setEnabled(true); // Example: Re-enable the whole viewer widget

        } else {
            qWarning() << "Indexing failed for" << logfile_ptr->getFileName();
            // Update tab title to show error
            tab_widget_ptr->setTabText(tab_index, "Error Loading");
            // Optionally show an error message within the viewer widget itself
            // viewer_ptr->showError("Failed to load log file."); // Example
            viewer_ptr->setEnabled(false); // Keep viewer disabled
        }
    });

    // Connect progress signal (optional, could update tab text or a progress bar in the viewer)
    QObject::connect(logfile_data, &::Logfile::indexingProgress, viewer,
        [tab_widget_ptr, tab_index](int percent) {
        if (!tab_widget_ptr) return;
        // Update tab text to show progress percentage
        QString current_text = tab_widget_ptr->tabText(tab_index);
        // Avoid flickering if text already contains progress
        if (!current_text.contains('%')) {
             // Use the short filename stored earlier as base if available
             QString base_text = current_text; // Fallback to current text
             if (tab_widget_ptr->widget(tab_index)) { // Check if widget exists
                 // Attempt to get original short name if stored somewhere, otherwise use current
                 // For now, just use the current text before the potential parenthesis
                 base_text = current_text.section('(', 0, 0).trimmed();
             }
             if (base_text.isEmpty() || base_text == "Loading...") { // Handle initial state
                 base_text = QFileInfo(tab_widget_ptr->tabToolTip(tab_index)).fileName(); // Get from tooltip
                 if (base_text.isEmpty()) base_text = "Loading..."; // Final fallback
             }
             tab_widget_ptr->setTabText(tab_index, QString("%1 (%2%)").arg(base_text).arg(percent));
        } else {
             // More robust update needed if text format is complex
             // For simplicity, just update percentage part
             QString base_text = current_text.section('(', 0, 0).trimmed();
             tab_widget_ptr->setTabText(tab_index, QString("%1 (%2%)").arg(base_text).arg(percent));
        }
    });


    // Use public accessors getDeepestActiveTab() and getGrepHierarchy()
    // spawnViews(viewer->getDeepestActiveTab(), logfile_data->getGrepHierarchy()); // REMOVED - Relies on old structure and disabled grep
}

/* // REMOVED - Relies on old structure and disabled grep
void LogfileLoader::spawnViews(LogViewer* parent_tab, const GrepNode* node) // Renamed class
{
    if (node == nullptr || parent_tab == nullptr) return; // Add check for parent_tab

    // Use public accessor getChildren()
    for (GrepNode* child : node->getChildren())
    {
        // Use public method grep()
        LogViewer* spawned_viewer = parent_tab->grep(child);
        // Recurse only if a new viewer was successfully created (grep might return nullptr)
        if (spawned_viewer) {
            spawnViews(spawned_viewer, child);
        }
    }
}
*/

}  // namespace loader
