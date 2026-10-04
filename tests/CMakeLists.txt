include(ECMAddTests)

ecm_add_test(core/tst_version.cpp
    TEST_NAME tst_version
    LINK_LIBRARIES contribusage_core Qt6::Test)
