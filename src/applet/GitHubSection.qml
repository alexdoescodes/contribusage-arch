import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// GitHub's header with the login, then today, the streak and the year (SPEC §11.2).
ColumnLayout {
    property var github

    spacing: 6

    SectionHeader {
        title: "GitHub"
        symbol: "vcs-commit"
        subtitle: github.login
        updatedText: github.updatedText
        Layout.fillWidth: true
    }
    PlasmaComponents.Label { text: github.statsText; font: Kirigami.Theme.smallFont }
}
