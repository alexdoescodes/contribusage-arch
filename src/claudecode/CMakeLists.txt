add_library(contribusage_claudecode STATIC
    activityjob.cpp
    activityjob.h
    activityview.cpp
    activityview.h
    claudecodelimits.cpp
    claudecodelimits.h
    claudelocator.cpp
    claudelocator.h
    limitsjob.cpp
    limitsjob.h
    limitsview.cpp
    limitsview.h
    transcriptindex.cpp
    transcriptindex.h
    transcriptline.cpp
    transcriptline.h
    usageparser.cpp
    usageparser.h)
target_include_directories(contribusage_claudecode PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(contribusage_claudecode PUBLIC contribusage_core)
# Linked into the shared applet library.
set_target_properties(contribusage_claudecode PROPERTIES POSITION_INDEPENDENT_CODE ON)
