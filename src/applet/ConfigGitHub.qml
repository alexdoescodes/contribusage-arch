import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

// SPEC §11.6 GitHub: switch, account, token (validated, then KWallet), interval.
KCM.SimpleKCM {
    property bool cfg_githubEnabled
    property int cfg_githubIntervalMinutes

    Kirigami.FormLayout {
        QQC2.Switch {
            Kirigami.FormData.label: "GitHub:"
            text: checked ? "On" : "Off"
            checked: cfg_githubEnabled
            onToggled: cfg_githubEnabled = checked
            Accessible.name: "GitHub on or off"
        }
        QQC2.Label {
            Kirigami.FormData.label: "Account:"
            text: (PopupModel.github.login ?? "").length > 0 ? PopupModel.github.login : "Not connected"
        }
        RowLayout {
            Kirigami.FormData.label: (PopupModel.github.login ?? "").length > 0 ? "Replace token:" : "Token:"
            Kirigami.PasswordField {
                id: tokenField
                Layout.fillWidth: true
                Accessible.name: "GitHub token"
            }
            QQC2.Button {
                text: (PopupModel.github.login ?? "").length > 0 ? "Replace" : "Connect"
                enabled: tokenField.text.trim().length > 0
                onClicked: {
                    const token = tokenField.text.trim();
                    tokenField.text = "";
                    PopupModel.connectGitHub(token);
                }
            }
        }
        QQC2.Label {
            visible: (PopupModel.github.connectError ?? "").length > 0
            text: PopupModel.github.connectError ?? ""
            color: Kirigami.Theme.negativeTextColor
        }
        QQC2.Button {
            visible: (PopupModel.github.login ?? "").length > 0
            text: "Disconnect"
            onClicked: PopupModel.disconnectGitHub()
        }
        Kirigami.UrlButton {
            url: "https://github.com/settings/personal-access-tokens/new"
            text: "Create a read-only token"
        }
        QQC2.SpinBox {
            Kirigami.FormData.label: "Refresh every (min):"
            from: 10
            to: 360
            value: cfg_githubIntervalMinutes
            onValueModified: cfg_githubIntervalMinutes = value
            Accessible.name: "GitHub refresh interval in minutes"
        }
    }
}
