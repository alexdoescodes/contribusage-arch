import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents

// One provider: header, then its sections in the layout's order (FR-4, FR-44, ADR-034; SPEC §11.2).
ColumnLayout {
    id: group

    property var provider
    property QtObject insightsState: null

    spacing: Theme.spacing

    SectionHeader {
        title: provider.name
        symbol: provider.symbol
        updatedText: provider.updatedText
        Layout.fillWidth: true
    }
    Repeater {
        model: group.provider.sections ?? ["limits", "activity", "insights"]
        Loader {
            required property string modelData
            active: modelData === "limits" || (modelData === "activity" && group.provider.activity !== undefined)
                || (modelData === "insights" && group.provider.insights !== undefined)
            visible: active
            Layout.fillWidth: true
            sourceComponent: modelData === "activity" ? activitySection
                : modelData === "insights" ? insightsSection : limitsSection
        }
    }

    Component {
        id: limitsSection
        ColumnLayout {
            spacing: Theme.spacing

            PlasmaComponents.Label {
                visible: (group.provider.statusText ?? "").length > 0
                text: group.provider.statusText ?? ""
                wrapMode: Text.Wrap
                opacity: Theme.secondaryOpacity
                Layout.fillWidth: true
            }
            ActionButton {
                stateAction: group.provider.action
                onRaw: rawOutput.visible = !rawOutput.visible
            }
            QQC2.TextArea {
                id: rawOutput
                visible: false
                readOnly: true
                wrapMode: TextEdit.Wrap
                text: group.provider.rawOutput ?? ""
                font.family: "monospace"
                Accessible.name: "Raw output"
                Layout.fillWidth: true
            }
            Skeleton {
                visible: group.provider.skeleton === true
                rows: 2
                Layout.fillWidth: true
            }
            Repeater {
                model: group.provider.windows
                LimitBar {
                    required property var modelData
                    limit: modelData
                    opacity: group.provider.stale ? 0.5 : 1
                    Layout.fillWidth: true
                }
            }
        }
    }
    Component {
        id: activitySection
        ActivitySection { activity: group.provider.activity }
    }
    Component {
        id: insightsSection
        InsightsSection {
            insights: group.provider.insights
            viewState: group.insightsState
        }
    }
}
