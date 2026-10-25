# UI 可执行程序

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.ui
)

add_executable(${PROJECT_NAME}
    ${APP_SOURCES}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        cbct_core
        Qt6::Widgets
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
)
