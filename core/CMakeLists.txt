# 核心动态库：暂为空骨架，后续逐步填充

# 共享库至少需要一个编译单元
add_library(cbct_core SHARED
    core_stub.cpp
)

target_include_directories(cbct_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 预留依赖：后续核心逻辑若用到再启用
# target_link_libraries(cbct_core
#     PUBLIC
#         Qt6::Core
#         Qt6::Gui
#         Qt6::Network
# )

set_target_properties(cbct_core PROPERTIES
    VERSION 0.1.0
    SOVERSION 0
    OUTPUT_NAME cbct_core
)
