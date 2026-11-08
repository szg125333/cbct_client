cmake_minimum_required(VERSION 3.16)
project(cbct_client LANGUAGES C CXX)

# 默认 Release：空 CMAKE_BUILD_TYPE 等价 -O0，测量预览/拾取会明显卡顿
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "CMAKE_BUILD_TYPE 默认设为 Release")
endif()

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
# Concurrent：DICOM 序列加载放工作线程，UI 不卡
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql Network Concurrent)
find_package(spdlog REQUIRED)

# DICOM 解析（core/dicom 用；GDCM 读像素与序列，DCMTK 读元数据）
# Ubuntu 24.04 的 libgdcm-dev 把 GDCMConfig.cmake 放在非默认路径，需显式 PATHS
find_package(DCMTK REQUIRED)
message(STATUS "DCMTK version: ${DCMTK_VERSION}, dir: ${DCMTK_DIR}")
find_package(GDCM REQUIRED PATHS
    /usr/lib/x86_64-linux-gnu/gdcm-3.0
    /usr/lib/cmake/gdcm
    /usr/local/lib/cmake/gdcm
)
message(STATUS "GDCM version: ${GDCM_VERSION}, dir: ${GDCM_DIR}")
# 注意：不要 include(UseGDCM.cmake)，避免拖入 GDCM 自带的 VTK 绑定造成双 VTK

# VTK + Qt GUI（QVTKOpenGLNativeWidget 用于显示 vtkImageData）
set(VTK_QT_VERSION 5 CACHE STRING "Qt version for VTK" FORCE)
find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    ImagingCore
    ImagingGeneral
    ImagingMath
    InteractionStyle
    InteractionImage
    RenderingCore
    RenderingOpenGL2
    RenderingUI
    RenderingFreeType
    RenderingVolume
    RenderingVolumeOpenGL2
    GUISupportQt
)
message(STATUS "VTK version: ${VTK_VERSION}, Qt: ${VTK_QT_VERSION}")

add_subdirectory(core)
add_subdirectory(app)
