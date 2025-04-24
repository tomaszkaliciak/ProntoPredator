set(projectName "ProntoPredator")
set(version \"0.0.3\")

cmake_minimum_required(VERSION 3.9.0)
project (${projectName})
add_definitions(-DAPP_VERSION=${version})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS . src src/ui)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Concurrent) # Added Concurrent

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



set(SOURCES
    src/Bookmark.cpp
    src/BookmarksModel.cpp
    src/GrepDialogWindow.cpp
    src/GrepNode.cpp
    src/LogViewer.cpp
    src/CustomLogView.cpp # Added custom view source
    src/LogfileModel.cpp
    src/LogFilterProxyModel.cpp
    src/GrepModel.cpp # Added
    src/TextSelectionDelegate.cpp # Added delegate source
    src/MainWindow.cpp
    src/Logfile.cpp
    src/ProjectUiManager.cpp
    src/FileViewer.cpp
    src/main.cpp
    src/ProjectModel.cpp
    src/loader/Project.cpp
    src/loader/LoaderLogFile.cpp
    src/serializer/SerializerBookmark.cpp
    src/serializer/SerializerBookmarksModel.cpp
    src/serializer/SerializerGrepNode.cpp
    src/serializer/SerializerLogfile.cpp
    src/serializer/SerializerProjectModel.cpp
    src/EfficientLogFilterProxyModel.cpp # Added new efficient proxy model
    src/HighlightDialog.cpp # Added for custom highlighting
)

set(FORMS
    src/ui/GrepDialogWindow.ui
    src/ui/MainWindow.ui
    src/ui/HighlightDialog.ui # Added for custom highlighting
)

set(RESOURCES icons.qrc)

set(RC_FILE src/ui/app.rc)

include_directories(. src)

if(WIN32)
  set(CMAKE_WIN32_EXECUTABLE ON)
endif()

# Remove deprecated/redundant include_directories and add_definitions for Qt.
# target_link_libraries(Qt5::...) handles this automatically with modern CMake.
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}") # Deprecated variable usage
# include_directories(${Qt5Widgets_INCLUDE_DIRS} ${QtQml_INCLUDE_DIRS}) # Redundant with target_link_libraries
# add_definitions(${Qt5Widgets_DEFINITIONS} ${QtQml_DEFINITIONS} ${${Qt5Quick_DEFINITIONS}}) # Redundant with target_link_libraries

add_executable(${projectName} ${FORMS} ${RESOURCES} ${RC_FILE} ${SOURCES})

# Link libraries - this also sets up include paths and definitions for Qt5
target_link_libraries(${projectName} PUBLIC Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Concurrent) # Added Qt5::Concurrent
