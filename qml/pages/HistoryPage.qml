import QtQuick
import QtQuick.Controls.Basic
import PS5PackageSender

Page {
    background: Rectangle { color: Theme.background }

    Label {
        anchors.centerIn: parent
        text: "No transfers recorded"
        color: Theme.textMuted
        font.pixelSize: 14
    }
}
