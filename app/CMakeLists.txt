# UI 可执行程序（Qt5 + VTK）
#
# 目录约定：
#   ui/window/       主窗口、左侧栏、工具栏
#   ui/acquisition/  采集启动页、2D 采集显示、配方对话框
#   ui/view/         三视图等显示页
#   bridge/          app ↔ core 适配层

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.ui
)

set(APP_RESOURCES
    ${CMAKE_SOURCE_DIR}/resources/icons.qrc
)

add_executable(${PROJECT_NAME}
    ${APP_SOURCES}
    ${APP_RESOURCES}
)

# 各子目录都加入 include 路径，便于 #include "mainwindow.h" 等短名
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/window
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/acquisition
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/view
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        cbct_core
        Qt5::Widgets
        Qt5::Gui
        Qt5::Core
        ${VTK_LIBRARIES}
)

vtk_module_autoinit(
    TARGETS ${PROJECT_NAME}
    MODULES ${VTK_LIBRARIES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
)
