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

# 第三方 DICOM 库头文件以 SYSTEM 引入，压低其告警噪音
if(GDCM_INCLUDE_DIRS)
    target_include_directories(cbct_core SYSTEM PUBLIC ${GDCM_INCLUDE_DIRS})
endif()
if(DCMTK_INCLUDE_DIRS)
    target_include_directories(cbct_core SYSTEM PUBLIC ${DCMTK_INCLUDE_DIRS})
endif()

target_link_libraries(cbct_core
    PUBLIC
        Qt5::Core
        Qt5::Gui
        Qt5::Sql
        Qt5::Network
        spdlog::spdlog
        gdcmMSFF
        gdcmCommon
        gdcmDICT
        gdcmDSED
        gdcmIOD
        DCMTK::dcmdata
        DCMTK::ofstd
        DCMTK::oflog
)

set_target_properties(cbct_core PROPERTIES
    VERSION 0.1.0
    SOVERSION 0
    OUTPUT_NAME cbct_core
)
