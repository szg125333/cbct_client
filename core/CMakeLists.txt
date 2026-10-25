# 核心动态库：配方/客户端门面等（不依赖 Qt Widgets）

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)
file(GLOB_RECURSE CORE_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(cbct_core SHARED
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

target_include_directories(cbct_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cbct_core
    PUBLIC
        Qt5::Core
        Qt5::Gui
        Qt5::Sql
        spdlog::spdlog
)

set_target_properties(cbct_core PROPERTIES
    VERSION 0.1.0
    SOVERSION 0
    OUTPUT_NAME cbct_core
)
