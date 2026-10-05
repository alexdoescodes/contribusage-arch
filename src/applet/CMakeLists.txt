set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

plasma_add_applet(org.contribusage.widget
    QML_SOURCES main.qml Theme.qml SectionHeader.qml LimitBar.qml ActivitySection.qml InsightsSection.qml
        ProviderGroup.qml HeatmapBlock.qml GitHubSection.qml Footer.qml FullPopup.qml
        PromptGlyph.qml CompactLabel.qml Skeleton.qml ActionButton.qml
        LabelContent.qml RingGlyph.qml LineGlyph.qml MiniHeatmap.qml
    CPP_SOURCES appinfo.cpp appinfo.h popupmodel.cpp popupmodel.h
    GENERATE_APPLET_CLASS)
# The configuration schema at config/main.xml in the applet's resources, where Plasma looks for it.
ecm_target_qml_sources(org.contribusage.widget SOURCES config/main.xml PATH config)
target_link_libraries(org.contribusage.widget PRIVATE contribusage_claudecode contribusage_github contribusage_platform
    contribusage_core Qt6::Qml Qt6::Quick)
