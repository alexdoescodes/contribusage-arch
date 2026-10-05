import QtQuick
import org.kde.plasma.configuration

// SPEC §11.6's tabs that L-5a covers; the Popover tab follows with L-5b.
ConfigModel {
    ConfigCategory {
        name: "General"
        icon: "preferences-desktop"
        source: "ConfigGeneral.qml"
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
