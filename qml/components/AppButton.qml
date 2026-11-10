import QtQuick
import QtQuick.Controls.Basic
import PS5PackageSender

Button {
    id: control

    implicitHeight: Theme.controlHeight
    opacity: enabled ? 1 : 0.5

    background: Rectangle {
        radius: Theme.controlRadius
        color: control.highlighted
               ? (control.down ? Qt.darker(Theme.accent, 1.2) : Theme.accent)
               : control.checked
                 ? Theme.accentSoft
                 : control.flat
                   ? (control.hovered ? Theme.accentSoft : "transparent")
                   : control.down ? Theme.border
                   : control.hovered ? Theme.chip : Theme.surfaceAlt
        border.width: control.activeFocus ? 2 : control.checked ? 1 : 0
        border.color: control.highlighted ? Theme.text : Theme.accent
    }

    contentItem: Label {
        text: control.text
        font: control.font
        color: control.highlighted ? "#ffffff"
             : control.checked ? Theme.accentText
             : Theme.text
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }
}
