add_library(contribusage_smalltv STATIC
    smalltvpayload.cpp
    smalltvpayload.h
    smalltvserver.cpp
    smalltvserver.h)
target_include_directories(contribusage_smalltv PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(contribusage_smalltv PUBLIC contribusage_core Qt6::Network)
# Linked into the shared applet library.
set_target_properties(contribusage_smalltv PROPERTIES POSITION_INDEPENDENT_CODE ON)
