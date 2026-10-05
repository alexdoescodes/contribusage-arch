import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

// SPEC §11.6 Advanced: data folder, reset caches (never history or notification keys, ADR-028).
KCM.SimpleKCM {
    Kirigami.FormLayout {
        QQC2.Button {
            Kirigami.FormData.label: "Data:"
            text: "Open data folder"
            onClicked: Qt.openUrlExternally("file://" + PopupModel.dataFolder)
        }
        QQC2.Button {
            text: "Reset caches"
            onClicked: PopupModel.resetCaches()
        }
        QQC2.Label {
            text: "Resetting keeps the activity history and the record of sent notifications."
            wrapMode: Text.Wrap
            opacity: 0.7
        }
    }
}
