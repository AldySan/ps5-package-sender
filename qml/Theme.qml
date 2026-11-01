pragma Singleton
import QtQuick

QtObject {
    property bool dark: true

    readonly property color background: dark ? "#17181d" : "#f3f4f6"
    readonly property color surface: dark ? "#1f2128" : "#ffffff"
    readonly property color surfaceAlt: dark ? "#262932" : "#e9ebef"
    readonly property color field: dark ? "#1b1d24" : "#ffffff"
    readonly property color border: dark ? "#33363f" : "#d5d8df"
    readonly property color text: dark ? "#e8eaf0" : "#1d2129"
    readonly property color textMuted: dark ? "#9aa0ae" : "#667085"
    readonly property color accent: "#2d7ff9"
    readonly property color accentSoft: dark ? "#1a2d7ff9" : "#242d7ff9"
    readonly property color ok: dark ? "#3ecf8e" : "#1f9d61"
    readonly property color warn: dark ? "#f5b83d" : "#b97d10"
    readonly property color error: dark ? "#f0566a" : "#d23649"
    readonly property color chip: dark ? "#2b2e38" : "#e4e7ec"
    readonly property color toolTipBase: dark ? "#262932" : "#33373f"
    readonly property color toolTipText: dark ? "#e8eaf0" : "#f0f2f6"
    readonly property color toast: dark ? "#262932" : "#20232a"

    readonly property int radius: 8
    readonly property int spacing: 10
    readonly property int controlHeight: 36

    readonly property string mono: "Courier New"
}
