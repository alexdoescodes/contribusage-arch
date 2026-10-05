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
    LINK_LIBRARIES contribusage_core Qt6::Qml Qt6::Quick Qt6::Gui Qt6::Test)
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

ecm_add_test(core/tst_reports.cpp
    TEST_NAME tst_reports
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(core/tst_snapshot.cpp
    TEST_NAME tst_snapshot
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(core/tst_jsonstore.cpp
    TEST_NAME tst_jsonstore
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(core/tst_processrunner.cpp
    TEST_NAME tst_processrunner
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(claudecode/tst_usageparser.cpp
    TEST_NAME tst_usageparser
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)
target_compile_definitions(tst_usageparser PRIVATE
    CONTRIBUSAGE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/claudecode/fixtures")

ecm_add_test(claudecode/tst_claudelocator.cpp
    TEST_NAME tst_claudelocator
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)
target_include_directories(tst_claudelocator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

ecm_add_test(claudecode/tst_claudecodelimits.cpp
    TEST_NAME tst_claudecodelimits
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)
target_include_directories(tst_claudecodelimits PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(tst_claudecodelimits PRIVATE
    CONTRIBUSAGE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/claudecode/fixtures")

ecm_add_test(core/tst_limitscache.cpp
    TEST_NAME tst_limitscache
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(claudecode/tst_limitsjob.cpp
    TEST_NAME tst_limitsjob
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)
target_include_directories(tst_limitsjob PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(tst_limitsjob PRIVATE
    CONTRIBUSAGE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/claudecode/fixtures")

ecm_add_test(claudecode/tst_limitsview.cpp
    TEST_NAME tst_limitsview
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)

ecm_add_test(github/tst_githubreport.cpp
    TEST_NAME tst_githubreport
    LINK_LIBRARIES contribusage_github Qt6::Test)

ecm_add_test(github/tst_githubclient.cpp
    TEST_NAME tst_githubclient
    LINK_LIBRARIES contribusage_github Qt6::Test)
target_include_directories(tst_githubclient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(tst_githubclient PRIVATE
    CONTRIBUSAGE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/github/fixtures")

ecm_add_test(github/tst_githubaccount.cpp
    TEST_NAME tst_githubaccount
    LINK_LIBRARIES contribusage_github Qt6::Test)
target_include_directories(tst_githubaccount PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(tst_githubaccount PRIVATE
    CONTRIBUSAGE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/github/fixtures")


ecm_add_test(platform/tst_networktransport.cpp
    TEST_NAME tst_networktransport
    LINK_LIBRARIES contribusage_platform Qt6::Test)

ecm_add_test(github/tst_githubjob.cpp
    TEST_NAME tst_githubjob
    LINK_LIBRARIES contribusage_github Qt6::Test)
target_include_directories(tst_githubjob PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(tst_githubjob PRIVATE
    CONTRIBUSAGE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/github/fixtures")

ecm_add_test(github/tst_githubview.cpp
    TEST_NAME tst_githubview
    LINK_LIBRARIES contribusage_github Qt6::Test)


ecm_add_test(core/tst_jsonlreader.cpp
    TEST_NAME tst_jsonlreader
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(core/tst_historystore.cpp
    TEST_NAME tst_historystore
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(core/tst_directorywatcher.cpp
    TEST_NAME tst_directorywatcher
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(claudecode/tst_transcriptline.cpp
    TEST_NAME tst_transcriptline
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)

ecm_add_test(claudecode/tst_transcriptindex.cpp
    TEST_NAME tst_transcriptindex
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)

ecm_add_test(claudecode/tst_activityjob.cpp
    TEST_NAME tst_activityjob
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)
target_include_directories(tst_activityjob PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

ecm_add_test(claudecode/tst_activityview.cpp
    TEST_NAME tst_activityview
    LINK_LIBRARIES contribusage_claudecode contribusage_github Qt6::Test)

ecm_add_test(core/tst_schedule.cpp
    TEST_NAME tst_schedule
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(core/tst_sourceschedule.cpp
    TEST_NAME tst_sourceschedule
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(platform/tst_systemconditions.cpp
    TEST_NAME tst_systemconditions
    LINK_LIBRARIES contribusage_platform Qt6::Test)

ecm_add_test(core/tst_notificationplanner.cpp
    TEST_NAME tst_notificationplanner
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(core/tst_menubarlabel.cpp
    TEST_NAME tst_menubarlabel
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(core/tst_notificationstore.cpp
    TEST_NAME tst_notificationstore
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(core/tst_widgetsettings.cpp
    TEST_NAME tst_widgetsettings
    LINK_LIBRARIES contribusage_core Qt6::Test)

ecm_add_test(core/tst_popoverlayout.cpp
    TEST_NAME tst_popoverlayout
    LINK_LIBRARIES contribusage_core Qt6::Test)
