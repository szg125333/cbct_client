# UI 可执行程序（Qt5 + VTK）

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

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        cbct_core
        Qt5::Widgets
        Qt5::Gui
        Qt5::Core
        ${VTK_LIBRARIES}
)

# VTK 自动初始化模块（含 Qt 绑定）
vtk_module_autoinit(
    TARGETS ${PROJECT_NAME}
    MODULES ${VTK_LIBRARIES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
)
