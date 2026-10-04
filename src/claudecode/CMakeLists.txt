add_library(contribusage_claudecode STATIC claudelocator.cpp claudelocator.h usageparser.cpp usageparser.h)
    target_include_directories(contribusage_claudecode PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} /..)
        target_link_libraries(contribusage_claudecode PUBLIC contribusage_core)
#Linked into the shared applet library.
            set_target_properties(contribusage_claudecode PROPERTIES POSITION_INDEPENDENT_CODE ON)
