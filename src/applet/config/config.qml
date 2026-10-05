import QtQuick
import org.kde.plasma.configuration

// SPEC §11.6's tabs.
ConfigModel {
    ConfigCategory {
        name: "General"
        icon: "preferences-desktop"
        source: "ConfigGeneral.qml"
    }
    ConfigCategory {
        name: "Popover"
        icon: "view-list-details"
        source: "ConfigPopover.qml"
    }
    ConfigCategory {
        name: "Claude Code"
        icon: "draw-star"
        source: "ConfigClaudeCode.qml"
    }
    ConfigCategory {
        name: "GitHub"
        icon: "vcs-commit"
        source: "ConfigGitHub.qml"
    }
    ConfigCategory {
        name: "Advanced"
        icon: "configure"
        source: "ConfigAdvanced.qml"
    }
}
