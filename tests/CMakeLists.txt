include(ECMAddTests)

ecm_add_test(core/tst_version.cpp
    TEST_NAME tst_version
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(applet/tst_appletplugin.cpp
    TEST_NAME tst_appletplugin
    LINK_LIBRARIES KF6::CoreAddons Qt6::Test)
target_compile_definitions(tst_appletplugin PRIVATE CONTRIBUSAGE_PLUGIN_DIR="${CMAKE_BINARY_DIR}/bin")
add_dependencies(tst_appletplugin org.contribusage.widget)

ecm_add_test(applet/tst_appletqml.cpp
    TEST_NAME tst_appletqml
    LINK_LIBRARIES Qt6::Qml Qt6::Gui Qt6::Test)
target_compile_definitions(tst_appletqml PRIVATE
    CONTRIBUSAGE_APPLET_PATH="$<TARGET_FILE:org.contribusage.widget>")
set_tests_properties(tst_appletqml PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
add_dependencies(tst_appletqml org.contribusage.widget)

ecm_add_test(core/tst_display.cpp
    TEST_NAME tst_display
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(core/tst_heatmap.cpp
    TEST_NAME tst_heatmap
    LINK_LIBRARIES contribusage_core Qt6::Test)
