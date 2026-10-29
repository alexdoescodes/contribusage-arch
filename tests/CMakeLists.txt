include(ECMAddTests)

ecm_add_test(applet/tst_appletplugin.cpp
    TEST_NAME tst_appletplugin
    LINK_LIBRARIES KF6::CoreAddons Qt6::Test)
target_compile_definitions(tst_appletplugin PRIVATE CONTRIBUSAGE_PLUGIN_DIR="${CMAKE_BINARY_DIR}/bin")
add_dependencies(tst_appletplugin org.contribusage.widget)

ecm_add_test(applet/tst_appletqml.cpp
    TEST_NAME tst_appletqml
    LINK_LIBRARIES contribusage_core Qt6::Qml Qt6::Quick Qt6::Gui Qt6::Test)
target_compile_definitions(tst_appletqml PRIVATE
    CONTRIBUSAGE_APPLET_PATH="$<TARGET_FILE:org.contribusage.widget>")
set_tests_properties(tst_appletqml PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
add_dependencies(tst_appletqml org.contribusage.widget)

ecm_add_test(core/tst_reports.cpp
    TEST_NAME tst_reports
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(core/tst_jsonstore.cpp
    TEST_NAME tst_jsonstore
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(claudecode/tst_usageparser.cpp
    TEST_NAME tst_usageparser
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)
target_compile_definitions(tst_usageparser PRIVATE
    CONTRIBUSAGE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/claudecode/fixtures")

ecm_add_test(claudecode/tst_claudecodelimits.cpp
    TEST_NAME tst_claudecodelimits
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)
target_include_directories(tst_claudecodelimits PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(tst_claudecodelimits PRIVATE
    CONTRIBUSAGE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/claudecode/fixtures")

ecm_add_test(github/tst_githubclient.cpp
    TEST_NAME tst_githubclient
    LINK_LIBRARIES contribusage_github Qt6::Test)
target_include_directories(tst_githubclient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(tst_githubclient PRIVATE
    CONTRIBUSAGE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/github/fixtures")

ecm_add_test(core/tst_schedule.cpp
    TEST_NAME tst_schedule
    LINK_LIBRARIES contribusage_core Qt6::Test)

# L-7: the installed file list and the packaging files.
add_test(NAME install_layout
    COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR} -DSTAGE=${CMAKE_CURRENT_BINARY_DIR}/install-stage
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/packaging/check_install.cmake)
