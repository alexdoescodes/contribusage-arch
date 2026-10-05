import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

// SPEC §11.6 Popover: block order and visibility, heatmap membership, a provider's sections and their order, and the
// heatmap style. Up and down buttons stand in for drag and drop.
KCM.SimpleKCM {
    property string cfg_popoverLayout
    readonly property var rows: PopupModel.layoutRows(cfg_popoverLayout)

    ColumnLayout {
        Repeater {
            model: rows
            ColumnLayout {
                required property var modelData
                required property int index
                Layout.fillWidth: true

                RowLayout {
                    QQC2.CheckBox {
                        text: modelData.name
                        checked: modelData.shown
                        onToggled: cfg_popoverLayout = PopupModel.layoutSetShown(cfg_popoverLayout, modelData.id, checked)
                        Layout.fillWidth: true
                    }
                    QQC2.CheckBox {
                        visible: modelData.inHeatmap !== undefined
                        text: "In heatmap"
                        checked: modelData.inHeatmap === true
                        onToggled: cfg_popoverLayout = PopupModel.layoutSetInHeatmap(cfg_popoverLayout, modelData.id, checked)
                    }
                    QQC2.ToolButton {
                        icon.name: "go-up"
                        enabled: index > 0
                        Accessible.name: "Move " + modelData.name + " up"
                        onClicked: cfg_popoverLayout = PopupModel.layoutMoveBlock(cfg_popoverLayout, index, index - 1)
                    }
                    QQC2.ToolButton {
                        icon.name: "go-down"
                        enabled: index < rows.length - 1
                        Accessible.name: "Move " + modelData.name + " down"
                        onClicked: cfg_popoverLayout = PopupModel.layoutMoveBlock(cfg_popoverLayout, index, index + 1)
                    }
                }
                Repeater {
                    model: modelData.sections ?? []
                    RowLayout {
                        required property var modelData
                        required property int index
                        readonly property string provider: parent.modelData.id
                        Layout.leftMargin: Kirigami.Units.gridUnit
                        QQC2.CheckBox {
                            text: modelData.name.charAt(0).toUpperCase() + modelData.name.slice(1)
                            checked: modelData.shown
                            onToggled: cfg_popoverLayout = PopupModel.layoutSetSectionShown(cfg_popoverLayout, provider, modelData.name, checked)
                            Layout.fillWidth: true
                        }
                        QQC2.ToolButton {
                            icon.name: "go-up"
                            enabled: index > 0
                            Accessible.name: "Move " + modelData.name + " up"
                            onClicked: cfg_popoverLayout = PopupModel.layoutMoveSection(cfg_popoverLayout, provider, index, index - 1)
                        }
                        QQC2.ToolButton {
                            icon.name: "go-down"
                            enabled: index < 2
                            Accessible.name: "Move " + modelData.name + " down"
                            onClicked: cfg_popoverLayout = PopupModel.layoutMoveSection(cfg_popoverLayout, provider, index, index + 1)
                        }
                    }
                }
            }
        }
        RowLayout {
            QQC2.Label { text: "Heatmap style:" }
            QQC2.ComboBox {
                model: ["Combined", "Stacked"]
                currentIndex: cfg_popoverLayout.indexOf("\"stacked\"") >= 0 ? 1 : 0
                onActivated: index => cfg_popoverLayout = PopupModel.layoutSetHeatmapStyle(cfg_popoverLayout, index === 1 ? "stacked" : "combined")
                Accessible.name: "Heatmap style"
            }
        }
    }
}
