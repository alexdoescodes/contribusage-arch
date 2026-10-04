include(ECMAddTests)

ecm_add_test(core/tst_version.cpp
    TEST_NAME tst_version
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(applet/tst_appletplugin.cpp
    TEST_NAME tst_appletplugin
    LINK_LIBRARIES KF6::CoreAddons Qt6::Test)
target_compile_definitions(tst_appletplugin PRIVATE CONTRIBUSAGE_PLUGIN_DIR="${CMAKE_BINARY_DIR}/bin")
add_dependencies(tst_appletplugin org.contribusage.widget)
