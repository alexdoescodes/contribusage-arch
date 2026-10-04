plasma_add_applet(org.contribusage.widget
    QML_SOURCES main.qml
    CPP_SOURCES appinfo.cpp appinfo.h
    GENERATE_APPLET_CLASS)
target_link_libraries(org.contribusage.widget PRIVATE contribusage_core Qt6::Qml Qt6::Quick)
