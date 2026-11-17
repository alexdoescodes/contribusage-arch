import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

// ADR-051: the opt-in endpoint a SmallTV clock polls for the Claude Code limits.
KCM.SimpleKCM {
    property bool cfg_smalltvEnabled
    property string cfg_smalltvAddress
    property int cfg_smalltvPort

    Kirigami.FormLayout {
        QQC2.Switch {
            Kirigami.FormData.label: "Serve the limits:"
            text: "Let a SmallTV clock read the limits from this widget"
            checked: cfg_smalltvEnabled
            onToggled: cfg_smalltvEnabled = checked
            Accessible.name: "Serve the limits to a SmallTV"
        }
        QQC2.TextField {
            Kirigami.FormData.label: "Listen on:"
            enabled: cfg_smalltvEnabled
            text: cfg_smalltvAddress
            placeholderText: "127.0.0.1"
            onEditingFinished: cfg_smalltvAddress = text.trim()
            Accessible.name: "Listen address"
        }
        QQC2.SpinBox {
            Kirigami.FormData.label: "Port:"
            enabled: cfg_smalltvEnabled
            from: 1024
            to: 65535
            value: cfg_smalltvPort
            onValueModified: cfg_smalltvPort = value
            Accessible.name: "Port"
        }
        QQC2.Label {
            visible: PopupModel.smalltvStatus.length > 0
            text: PopupModel.smalltvStatus
            color: Kirigami.Theme.negativeTextColor
            wrapMode: Text.Wrap
        }
        QQC2.Label {
            text: "The clock polls http://<this address>:<port>/ and gets the session and week percentages and their reset times. Use this computer's LAN address, such as 192.168.1.20, so the clock can reach it; 127.0.0.1 only serves this computer. Anyone on that network can read the answer. It carries no credentials."
            wrapMode: Text.Wrap
            opacity: 0.7
        }
    }
}
