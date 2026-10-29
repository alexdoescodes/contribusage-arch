import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// GitHub's header with the login, then today, the streak and the year (SPEC §11.2). Without a saved token it shows the
// token form instead (ADR-043): the token goes to PopupModel.connectGitHub and the field is cleared at once.
ColumnLayout {
    id: section
    objectName: "githubBuiltIn"

    property var github
    readonly property bool connected: github.connected !== false && github.needsToken !== true

    signal tokenSubmitted(string token)

    function submit() {
        const token = tokenField.text.trim();
        tokenField.text = "";
        if (token.length > 0) {
            section.tokenSubmitted(token);
        }
    }

    spacing: 6

    RowLayout {
        Layout.fillWidth: true
        SectionHeader {
            title: "GitHub"
            symbol: "vcs-commit"
            subtitle: github.login ?? ""
            updatedText: github.updatedText ?? ""
            Layout.fillWidth: true
        }
        PlasmaComponents.ToolButton {
            visible: section.connected && github.canDisconnect === true
            icon.name: "network-disconnect"
            display: QQC2.AbstractButton.IconOnly
            text: "Disconnect GitHub"
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            onClicked: PopupModel.disconnectGitHub()
        }
    }
    PlasmaComponents.Label {
        topPadding: Theme.labelInset
        bottomPadding: Theme.labelInset
        visible: section.connected && (github.statsText ?? "").length > 0
        text: github.statsText ?? ""
        font: Kirigami.Theme.smallFont
        opacity: github.stale ? 0.5 : 1
    }
    PlasmaComponents.Label {
        topPadding: Theme.labelInset
        bottomPadding: Theme.labelInset
        visible: (github.statusText ?? "").length > 0
        text: github.statusText ?? ""
        font: Kirigami.Theme.smallFont
        opacity: Theme.secondaryOpacity
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    ActionButton { stateAction: github.action }
    Skeleton {
        visible: github.skeleton === true
        rows: 1
        Layout.fillWidth: true
    }
    RowLayout {
        visible: !section.connected
        Layout.fillWidth: true
        PlasmaComponents.TextField {
            id: tokenField
            objectName: "tokenField"
            placeholderText: "Paste a GitHub token"
            echoMode: TextInput.Password
            enabled: github.busy !== true
            Layout.fillWidth: true
            onAccepted: section.submit()
        }
        PlasmaComponents.Button {
            text: "Connect"
            enabled: tokenField.text.length > 0 && github.busy !== true
            onClicked: section.submit()
        }
    }
    PlasmaComponents.Label {
        topPadding: Theme.labelInset
        bottomPadding: Theme.labelInset
        visible: !section.connected && (github.connectError ?? "").length > 0
        text: github.connectError ?? ""
        font: Kirigami.Theme.smallFont
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    PlasmaComponents.Label {
        topPadding: Theme.labelInset
        bottomPadding: Theme.labelInset
        visible: !section.connected
        text: "<a href=\"https://github.com/settings/personal-access-tokens/new\">Create a read-only token</a>"
        font: Kirigami.Theme.smallFont
        onLinkActivated: link => Qt.openUrlExternally(link)
    }
}
