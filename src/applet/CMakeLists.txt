set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

plasma_add_applet(org.contribusage.widget
    QML_SOURCES main.qml Theme.qml SectionHeader.qml LimitBar.qml ActivitySection.qml InsightsSection.qml
        ProviderGroup.qml HeatmapBlock.qml GitHubSection.qml Footer.qml FullPopup.qml
    CPP_SOURCES appinfo.cpp appinfo.h popupmodel.cpp popupmodel.h
    GENERATE_APPLET_CLASS)
target_link_libraries(org.contribusage.widget PRIVATE contribusage_core Qt6::Qml Qt6::Quick)
