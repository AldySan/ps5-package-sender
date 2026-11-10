import QtQuick
import QtQuick.Controls.Basic
import PS5PackageSender

ComboBox {
    id: control

    implicitHeight: Theme.controlHeight

    background: Rectangle {
        radius: Theme.controlRadius
        color: Theme.field
        border.width: control.activeFocus || control.popup.visible ? 2 : 1
        border.color: control.activeFocus || control.popup.visible
                      ? Theme.accent : Theme.border
    }

    contentItem: Label {
        leftPadding: 10
        rightPadding: control.indicator.width + 8
        text: control.displayText
        color: Theme.text
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideMiddle
    }

    delegate: ItemDelegate {
        id: entry
        required property string modelData
        required property int index
        width: ListView.view.width
        height: 34
        highlighted: control.highlightedIndex === index

        background: Rectangle {
            radius: Theme.controlRadius - 2
            color: entry.highlighted ? Theme.accentSoft : "transparent"
        }

        contentItem: Label {
            text: entry.modelData
            color: entry.highlighted ? Theme.accentText : Theme.text
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideMiddle
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        padding: 6
        height: Math.min(contentItem.implicitHeight + 2 * padding,
                         control.Window.window.height - 12)

        background: Rectangle {
            color: Theme.surface
            radius: Theme.controlRadius
            border.width: 1
            border.color: Theme.border
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            spacing: 2
            ScrollBar.vertical: ScrollBar {}
        }
    }
}
