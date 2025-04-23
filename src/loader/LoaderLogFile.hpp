#ifndef LOADER_LOGFILE_HPP
#define LOADER_LOGFILE_HPP

#include <functional>
#include <memory>

class FileViewer;
class LogViewer;
class Logfile; // Keep ::Logfile forward declaration
class GrepNode;
class QTabWidget; // Forward declare QTabWidget

// Remove Ui::MainWindow forward declaration
// namespace Ui { class MainWindow; }

namespace loader
{

class LogfileLoader // Renamed from Logfile
{
public:
    LogfileLoader() = delete; // Keep constructor deleted
    // Change signature to accept QTabWidget* instead of Ui::MainWindow*
    static void load(QTabWidget *target_tab_widget, ::Logfile* logfile_data, std::function<void(FileViewer*)> connect_slots_method);

protected:
    // spawnViews remains protected static (though currently commented out)
    static void spawnViews(LogViewer* parent_tab, const GrepNode* node);
};

}  // namespace loader

#endif // LOADER_LOGFILE_HPP
