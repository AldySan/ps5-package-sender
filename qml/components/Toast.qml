import QtQuick
import QtQuick.Controls.Basic
import PS5PackageSender

Rectangle {
    id: root
    property string message: ""

    function show(text) {
        root.message = text
        hideTimer.restart()
        root.state = "shown"
    }

    function hide() {
        hideTimer.stop()
        root.state = ""
    }

    visible: opacity > 0
    width: Math.min(420, (parent ? parent.width : 400) - 40)
    height: label.implicitHeight + 20
    radius: Theme.radius
    color: Theme.toast
    border.width: 1
    border.color: Theme.border
    anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
    anchors.bottom: parent ? parent.bottom : undefined
    anchors.bottomMargin: 18

    opacity: 0
    state: ""
    states: State {
        name: "shown"
        PropertyChanges { target: root; opacity: 1 }
        PropertyChanges { target: root; anchors.bottomMargin: 26 }
    }

    transitions: [
        Transition {
            from: ""; to: "shown"
            SequentialAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                NumberAnimation { property: "anchors.bottomMargin"; duration: 180; easing.type: Easing.OutCubic }
            }
        },
        Transition {
            from: "shown"; to: ""
            SequentialAnimation {
                NumberAnimation { property: "opacity"; to: 0; duration: 220; easing.type: Easing.InCubic }
                NumberAnimation { property: "anchors.bottomMargin"; to: 18; duration: 220; easing.type: Easing.InCubic }
            }
        }
    ]

    Label {
        id: label
        anchors.fill: parent
        anchors.margins: 10
        text: root.message
        color: Theme.text
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }

    Timer {
        id: hideTimer
        interval: 3500
        onTriggered: root.hide()
    }
}
