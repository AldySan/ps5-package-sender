pragma Singleton
import QtQuick

QtObject {
    readonly property color background: "#17181d"
    readonly property color surface: "#1f2128"
    readonly property color surfaceAlt: "#262932"
    readonly property color border: "#33363f"
    readonly property color text: "#e8eaf0"
    readonly property color textMuted: "#9aa0ae"
    readonly property color accent: "#2d7ff9"
    readonly property color accentSoft: "#2d7ff91a"
    readonly property color ok: "#3ecf8e"
    readonly property color warn: "#f5b83d"
    readonly property color error: "#f0566a"
    readonly property color chip: "#2b2e38"

    readonly property int radius: 8
    readonly property int spacing: 10
    readonly property int controlHeight: 36

    readonly property string mono: "Courier New"
}
