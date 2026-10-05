add_library(contribusage_github STATIC
    githubreport.cpp
    githubreport.h
    githubclient.cpp
    githubclient.h
    githubaccount.cpp
    githubaccount.h)
target_include_directories(contribusage_github PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(contribusage_github PUBLIC contribusage_core)
# Linked into the shared applet library.
set_target_properties(contribusage_github PROPERTIES POSITION_INDEPENDENT_CODE ON)
