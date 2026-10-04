import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// Symbol, title, an optional subtitle and the age of the section's data (SPEC §11.2).
RowLayout {
    property string title
    property string symbol
    property string subtitle
    property string updatedText

    spacing: Theme.spacing

    Kirigami.Icon {
        source: parent.symbol
        Layout.preferredWidth: Kirigami.Units.iconSizes.small
        Layout.preferredHeight: Kirigami.Units.iconSizes.small
    }
    PlasmaComponents.Label {
        text: parent.title
        font.weight: Font.DemiBold
        Accessible.role: Accessible.Heading
    }
    PlasmaComponents.Label {
        visible: parent.subtitle.length > 0
        text: parent.subtitle
        opacity: Theme.secondaryOpacity
    }
    Item { Layout.fillWidth: true }
    PlasmaComponents.Label {
        text: parent.updatedText
        font: Kirigami.Theme.smallFont
        opacity: Theme.secondaryOpacity
    }
}
