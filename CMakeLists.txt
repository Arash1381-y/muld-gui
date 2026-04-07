cmake_minimum_required(VERSION 3.16)
project(muld-gui VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Svg)
find_package(muld REQUIRED)

set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/core/DownloadItem.cpp
    src/core/DownloadManager.cpp
    src/models/DownloadListModel.cpp
    src/controllers/DownloadController.cpp
    src/ui/mainwindow/MainWindow.cpp
    src/ui/dialogs/AddDownloadDialog.cpp
    src/ui/widgets/DownloadItemDelegate.cpp
    src/utils/FormatUtils.cpp
)

set(HEADERS
    src/app/Application.h
    src/core/DownloadItem.h
    src/core/DownloadManager.h
    src/core/DownloadTypes.h
    src/models/DownloadListModel.h
    src/controllers/DownloadController.h
    src/ui/mainwindow/MainWindow.h
    src/ui/dialogs/AddDownloadDialog.h
    src/ui/widgets/DownloadItemDelegate.h
    src/utils/FormatUtils.h
)

set(RESOURCES
    src/resources/resources.qrc
)

add_executable(muld-gui
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

target_link_libraries(muld-gui PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Svg
    muld::muld
)

target_include_directories(muld-gui PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
