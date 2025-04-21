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

class Logfile // This class name might be confusing, it's a loader, not the data model
{
public:
    Logfile() = delete;
    // Change signature to accept QTabWidget* instead of Ui::MainWindow*
    static void load(QTabWidget *target_tab_widget, ::Logfile* logfile_data, std::function<void(FileViewer*)> connect_slots_method);

protected:
    // spawnViews remains protected static
    static void spawnViews(LogViewer* parent_tab, const GrepNode* node);
};

}  // namespace loader

#endif // LOADER_LOGFILE_HPP
