cmake_minimum_required(VERSION 3.21)
project(ztf-designer VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RC_COMPILER "D:/QT/Tools/mingw1310_64/bin/windres.exe")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Concurrent PrintSupport Network)
qt_standard_project_setup()

file(GLOB_RECURSE CORE_SOURCES src/core/*.cpp src/core/*.h)
file(GLOB_RECURSE ENGINE_SOURCES src/engine/*.cpp src/engine/*.h)
file(GLOB_RECURSE ADAPTER_SOURCES src/adapter/*.cpp src/adapter/*.h)
file(GLOB_RECURSE GUI_SOURCES src/gui/*.cpp src/gui/*.h)

qt_add_executable(ztf-designer WIN32
    src/main.cpp
    ${CORE_SOURCES}
    ${ENGINE_SOURCES}
    ${ADAPTER_SOURCES}
    ${GUI_SOURCES}
    resources/resources.qrc
    app.rc
)

target_include_directories(ztf-designer PRIVATE
    src
    src/core
    src/engine
    src/adapter
    src/gui
    src/gui/pages
    src/gui/widgets
    src/gui/dialogs
)

target_link_libraries(ztf-designer PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Concurrent
    Qt6::PrintSupport
    Qt6::Network
)

set_target_properties(ztf-designer PROPERTIES
    OUTPUT_NAME "ZTBLD-Designer"
    WIN32_EXECUTABLE TRUE
)
