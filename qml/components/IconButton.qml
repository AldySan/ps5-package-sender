import QtQuick
import QtQuick.Controls.Basic
import PS5PackageSender

Button {
    id: control

    property string iconName: ""
    property real iconSize: 16

    implicitWidth: Math.max(28, iconSize + 14)
    implicitHeight: Math.max(28, iconSize + 14)

    Accessible.name: control.ToolTip.text.length > 0 ? control.ToolTip.text
                                                     : control.iconName
    ToolTip.visible: hovered

    background: Rectangle {
        radius: Theme.controlRadius
        color: control.down ? Theme.border
               : control.hovered ? Theme.accentSoft
               : "transparent"
        border.width: control.activeFocus ? 2 : 0
        border.color: Theme.accent
    }

    contentItem: ThemeIcon {
        name: control.iconName
        size: control.iconSize
        color: control.enabled ? Theme.text : Theme.textMuted
    }
}
