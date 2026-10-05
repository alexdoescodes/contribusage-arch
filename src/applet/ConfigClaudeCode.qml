import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

// SPEC §11.6 Providers, for Claude Code: path override, probe interval, delete its data (US-12).
KCM.SimpleKCM {
    property string cfg_claudePathOverride
    property int cfg_probeIntervalMinutes

    Kirigami.FormLayout {
        QQC2.TextField {
            Kirigami.FormData.label: "Path to claude:"
            text: cfg_claudePathOverride
            placeholderText: "found automatically"
            onEditingFinished: cfg_claudePathOverride = text.trim()
            Accessible.name: "Path to claude"
        }
        QQC2.SpinBox {
            Kirigami.FormData.label: "Check limits every (min):"
            from: 1
            to: 60
            value: cfg_probeIntervalMinutes
            onValueModified: cfg_probeIntervalMinutes = value
            Accessible.name: "Probe interval in minutes"
        }
        Item { Kirigami.FormData.isSection: true }
        QQC2.Button {
            text: "Delete data for Claude Code…"
            onClicked: confirm.open()
        }
        Kirigami.PromptDialog {
            id: confirm
            title: "Delete Claude Code data?"
            subtitle: "This removes the cached limits and the activity history kept by the widget. Claude Code's own files stay."
            standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
            onAccepted: PopupModel.deleteClaudeCodeData()
        }
    }
}
