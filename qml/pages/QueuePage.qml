import QtQuick
import QtQuick.Controls.Basic
import PS5PackageSender

Page {
    background: Rectangle { color: Theme.background }

    Label {
        anchors.centerIn: parent
        text: "Send queue is empty"
        color: Theme.textMuted
        font.pixelSize: 14
    }
}
