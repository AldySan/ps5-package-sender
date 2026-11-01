import QtQuick
import QtQuick.Controls.Basic
import PS5PackageSender

Rectangle {
    id: root
    property string message: ""

    function show(text) {
        root.message = text
        hideTimer.restart()
        root.visible = true
    }

    visible: false
    width: Math.min(420, (parent ? parent.width : 400) - 40)
    height: label.implicitHeight + 20
    radius: Theme.radius
    color: "#000000cc"
    border.width: 1
    border.color: Theme.border
    anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
    anchors.bottom: parent ? parent.bottom : undefined
    anchors.bottomMargin: 18

    Label {
        id: label
        anchors.fill: parent
        anchors.margins: 10
        text: root.message
        color: "#ffffff"
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }

    Timer {
        id: hideTimer
        interval: 3500
        onTriggered: root.visible = false
    }
}
