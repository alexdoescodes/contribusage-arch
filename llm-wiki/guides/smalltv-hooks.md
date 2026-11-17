---
type: guide
status: active
updated: 2026-11-17
tracks: [src/smalltv, src/applet/ConfigSmallTV.qml]
tags: [smalltv, hooks]
---
# Session states for the SmallTV: the Claude Code hooks

The SmallTV endpoint ([[modules/smalltv]]) can tell the clock that a Claude Code session waits for you ([[decisions/0052-session-states-from-hooks]]). Claude Code's own hooks write one small file per session; the widget only reads them.

| Hook | Writes |
|---|---|
| `UserPromptSubmit`, `PostToolUse` | `running` |
| `Stop`, `Notification` | `waiting` |
| `SessionEnd` | removes the file |

The files are `<data folder>/sessions/<session id>` (default `~/.local/share/contribusage/sessions/`). The id is cut down to letters, digits, `_` and `-` before it becomes a file name, so a hook cannot write outside the folder. A file nobody has touched for 2 hours is ignored. The commands need `jq`.

Merge this into the `hooks` of `~/.claude/settings.json` (keep any hooks already there); Claude Code loads hooks when a session starts, so open a new session afterwards. All commands are `async`, so a prompt never waits on them.

```json
{
  "hooks": {
    "UserPromptSubmit": [
      {
        "hooks": [
          {
            "type": "command",
            "async": true,
            "command": "jq -r '.session_id // empty | gsub(\"[^A-Za-z0-9_-]\";\"\")' | { read -r s; [ -n \"$s\" ] || exit 0; d=\"${XDG_DATA_HOME:-$HOME/.local/share}/contribusage/sessions\"; mkdir -p \"$d\" && echo running > \"$d/$s\"; }"
          }
        ]
      }
    ],
    "PostToolUse": [
      {
        "hooks": [
          {
            "type": "command",
            "async": true,
            "command": "jq -r '.session_id // empty | gsub(\"[^A-Za-z0-9_-]\";\"\")' | { read -r s; [ -n \"$s\" ] || exit 0; d=\"${XDG_DATA_HOME:-$HOME/.local/share}/contribusage/sessions\"; mkdir -p \"$d\" && echo running > \"$d/$s\"; }"
          }
        ]
      }
    ],
    "Stop": [
      {
        "hooks": [
          {
            "type": "command",
            "async": true,
            "command": "jq -r '.session_id // empty | gsub(\"[^A-Za-z0-9_-]\";\"\")' | { read -r s; [ -n \"$s\" ] || exit 0; d=\"${XDG_DATA_HOME:-$HOME/.local/share}/contribusage/sessions\"; mkdir -p \"$d\" && echo waiting > \"$d/$s\"; }"
          }
        ]
      }
    ],
    "Notification": [
      {
        "hooks": [
          {
            "type": "command",
            "async": true,
            "command": "jq -r '.session_id // empty | gsub(\"[^A-Za-z0-9_-]\";\"\")' | { read -r s; [ -n \"$s\" ] || exit 0; d=\"${XDG_DATA_HOME:-$HOME/.local/share}/contribusage/sessions\"; mkdir -p \"$d\" && echo waiting > \"$d/$s\"; }"
          }
        ]
      }
    ],
    "SessionEnd": [
      {
        "hooks": [
          {
            "type": "command",
            "async": true,
            "command": "jq -r '.session_id // empty | gsub(\"[^A-Za-z0-9_-]\";\"\")' | { read -r s; [ -n \"$s\" ] || exit 0; rm -f \"${XDG_DATA_HOME:-$HOME/.local/share}/contribusage/sessions/$s\"; }"
          }
        ]
      }
    ]
  }
}
```

Remove the five entries to stop; the widget then reports 0 waiting.
