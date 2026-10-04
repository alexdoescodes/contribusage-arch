add_library(contribusage_core STATIC
    display.cpp
    display.h
    heatmap.cpp
    heatmap.h
    version.cpp
    version.h)
target_compile_definitions(contribusage_core PRIVATE CONTRIBUSAGE_VERSION="${PROJECT_VERSION}")
target_include_directories(contribusage_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(contribusage_core PUBLIC Qt6::Core)
# Linked into the shared applet library.
set_target_properties(contribusage_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
