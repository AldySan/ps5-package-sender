import QtQuick
import QtQuick.Controls.Basic
import PS5PackageSender

TextField {
    id: control

    implicitHeight: Theme.controlHeight
    selectByMouse: true
    color: Theme.text
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.accent
    selectedTextColor: "#ffffff"

    background: Rectangle {
        radius: Theme.controlRadius
        color: Theme.field
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
}
