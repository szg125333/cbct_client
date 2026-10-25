cmake_minimum_required(VERSION 3.16)
project(cbct_client LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 产物统一输出到构建目录根，方便运行与调试
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 与系统 VTK 保持一致：Qt5（Ubuntu 的 libvtk9-qt 基于 Qt5 编译）
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql)
find_package(spdlog REQUIRED)

# VTK + Qt GUI（QVTKOpenGLNativeWidget 用于显示 vtkImageData）
set(VTK_QT_VERSION 5 CACHE STRING "Qt version for VTK" FORCE)
find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    ImagingCore
    InteractionStyle
    RenderingCore
    RenderingOpenGL2
    RenderingUI
    GUISupportQt
)
message(STATUS "VTK version: ${VTK_VERSION}, Qt: ${VTK_QT_VERSION}")

add_subdirectory(core)
add_subdirectory(app)
