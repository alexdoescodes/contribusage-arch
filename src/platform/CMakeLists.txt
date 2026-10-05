add_library(contribusage_platform STATIC
    kwalletsecretstore.cpp
    kwalletsecretstore.h
    networktransport.cpp
    networktransport.h
    desktopnotifier.cpp
    desktopnotifier.h
    systemconditions.cpp
    systemconditions.h)
target_include_directories(contribusage_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(contribusage_platform PUBLIC contribusage_core Qt6::DBus Qt6::Network KF6::Wallet)
# Linked into the shared applet library.
set_target_properties(contribusage_platform PROPERTIES POSITION_INDEPENDENT_CODE ON)
