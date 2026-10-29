import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

// SPEC §11.6 General: label mode, style (with a live preview), color, notifications.
KCM.SimpleKCM {
    property string cfg_menuBarMode
    property string cfg_menuBarStyle
    property string cfg_menuBarTint
    property string cfg_menuBarColor
    property string cfg_notificationThresholds
    property bool cfg_notifyOnReset

    readonly property var modes: [["primary", "Session"], ["weekly", "Week"], ["highest", "Highest window"],
        ["githubToday", "GitHub today"], ["primaryAndGitHub", "Session and GitHub"], ["iconOnly", "Icon only"]]
    readonly property var styles: [["prompt", "Prompt"], ["rings", "Rings"], ["ring", "Ring"], ["line", "Line"],
        ["heatmap", "Heatmap"], ["sharedHeatmap", "Shared heatmap"], ["text", "Text"]]
    readonly property var tints: [["provider", "Source color"], ["usage", "By usage"], ["accent", "Accent color"],
        ["monochrome", "Monochrome"], ["custom", "Custom"]]

    function indexOf(list, key) {
        for (let i = 0; i < list.length; ++i) {
            if (list[i][0] === key) return i;
        }
        return 0;
    }

    Kirigami.FormLayout {
        QQC2.ComboBox {
            Kirigami.FormData.label: "Panel shows:"
            model: modes.map(m => m[1])
            currentIndex: indexOf(modes, cfg_menuBarMode)
            onActivated: index => cfg_menuBarMode = modes[index][0]
            Accessible.name: "Panel shows"
        }
        QQC2.ComboBox {
            id: styleBox
            Kirigami.FormData.label: "Style:"
            model: styles.map(s => s[1])
            currentIndex: indexOf(styles, cfg_menuBarStyle)
            onActivated: index => cfg_menuBarStyle = styles[index][0]
            Accessible.name: "Label style"
        }
        LabelContent {
            Kirigami.FormData.label: "Preview:"
            style: cfg_menuBarStyle
            tint: cfg_menuBarTint
            customColor: cfg_menuBarColor
            label: PopupModel.label
            heatmap: PopupModel.heatmap
        }
        QQC2.ComboBox {
            Kirigami.FormData.label: "Color:"
            model: tints.map(t => t[1])
            currentIndex: indexOf(tints, cfg_menuBarTint)
            onActivated: index => cfg_menuBarTint = tints[index][0]
            Accessible.name: "Label color"
        }
        QQC2.TextField {
            Kirigami.FormData.label: "Custom color:"
            visible: cfg_menuBarTint === "custom"
            text: cfg_menuBarColor
            placeholderText: "#ff9f0a"
            onEditingFinished: cfg_menuBarColor = text.trim()
            Accessible.name: "Custom color"
        }
        Item { Kirigami.FormData.isSection: true }
        QQC2.TextField {
            Kirigami.FormData.label: "Notify at (%):"
            text: cfg_notificationThresholds
            placeholderText: "80,95"
            onEditingFinished: cfg_notificationThresholds = text
            Accessible.name: "Notification thresholds, up to three values between 50 and 99"
        }
        QQC2.CheckBox {
            text: "Notify when a window resets"
            checked: cfg_notifyOnReset
            onToggled: cfg_notifyOnReset = checked
        }
    }
}
