import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import PS5PackageSender

Page {
    id: page
    background: Rectangle { color: Theme.background }

    property bool hasSelection: app.libraryModel.selectedCount > 0
    property bool hasImageSelection: app.libraryModel.selectedImageCount > 0
    property bool hasPkgSelection: app.libraryModel.selectedPkgCount > 0
    property string filter: "All"

    FolderDialog {
        id: folderDialog
        currentFolder: app.lastPickFolder.length > 0
                        ? "file://" + app.lastPickFolder : ""
        onAccepted: {
            app.lastPickFolder = Qt.resolvedUrl(selectedFolder).toString().replace("file://", "")
            app.addFolder(selectedFolder)
        }
    }

    FileDialog {
        id: pkgDialog
        title: qsTr("Select packages and disc images")
        nameFilters: [qsTr("Packages and images (*.pkg *.fpkg *.exfat *.ffpkg *.ffpfs *.ffpfsc)")]
        fileMode: FileDialog.OpenFiles
        currentFolder: app.lastPickFolder.length > 0
                       ? "file://" + app.lastPickFolder : ""
        onAccepted: app.addDroppedUrls(selectedFiles)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            spacing: 10

            AppButton {
                text: qsTr("+ Add folder")
                enabled: !app.scanning
                onClicked: folderDialog.open()
            }

            AppButton {
                text: qsTr("+ Add PKG files")
                enabled: !app.scanning
                onClicked: pkgDialog.open()
            }

            AppButton {
                text: qsTr("Rescan")
                enabled: !app.scanning
                onClicked: app.rescanAll()
            }

            AppTextField {
                id: search
                Layout.preferredWidth: 260
                placeholderText: qsTr("Search title or ID")
                text: app.libraryModel.searchText
                onTextChanged: {
                    if (text !== app.libraryModel.searchText)
                        app.libraryModel.searchText = text
                }
            }

            Item { Layout.fillWidth: true }

            Label {
                text: app.scanning ? app.scanStatus
                     : app.libraryModel.count + qsTr(" item(s)")
                color: Theme.textMuted
                font.pixelSize: 12
            }
        }

        RowLayout {
            spacing: 8

            Repeater {
                model: ["All", "Game", "DLC", "Image"]

                delegate: Button {
                    id: filterChip
                    readonly property bool active: page.filter === modelData
                    flat: true
                    text: modelData
                    implicitHeight: 28
                    font.pixelSize: 12
                    onClicked: {
                        page.filter = modelData
                        app.libraryModel.formatFilter = modelData === "All" ? "" : modelData
                    }

                    background: Rectangle {
                        radius: 12
                        color: (active || filterChip.hovered) ? Theme.accentSoft : Theme.chip
                        border.width: active ? 1 : filterChip.activeFocus ? 2 : 0
                        border.color: Theme.accent
                    }

                    contentItem: Label {
                        text: parent.text
                        color: parent.active ? Theme.accentText : Theme.textMuted
                        font: parent.font
                    }
                }
            }

            Item { Layout.fillWidth: true }

            AppButton {
                visible: app.libraryModel.familyFilter.length > 0
                flat: true
                text: qsTr("↩ Back to all")
                font.pixelSize: 12
                onClicked: app.libraryModel.familyFilter = ""
            }

            AppButton {
                text: qsTr("Select all")
                flat: true
                font.pixelSize: 12
                onClicked: app.libraryModel.selectVisible(true)
            }

            AppButton {
                text: qsTr("Deselect")
                flat: true
                font.pixelSize: 12
                enabled: page.hasSelection
                onClicked: app.libraryModel.clearSelection()
            }

            AppButton {
                text: qsTr("Remove selected")
                flat: true
                font.pixelSize: 12
                enabled: page.hasSelection && !app.scanning
                onClicked: app.removeSelected()
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            visible: app.libraryModel.count > 0
            cellWidth: Math.floor(width / Math.max(3, Math.floor(width / 300)))
            cellHeight: 300
            model: app.libraryModel
            ScrollBar.vertical: ScrollBar {}

            delegate: Item {
                id: cell
                required property var model
                width: grid.cellWidth
                height: grid.cellHeight

                GameCard {
                    anchors.fill: parent
                    anchors.margins: 4
                    model: cell.model
                }
            }
        }

        Label {
            visible: app.libraryModel.count === 0
            Layout.fillHeight: true
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: app.scanning ? qsTr("Scanning…")
                 : qsTr("No packages yet: add a folder or drop PKG/FPKG files here")
            color: Theme.textMuted
            font.pixelSize: 14
            wrapMode: Text.WordWrap
        }

        RowLayout {
            visible: app.libraryModel.count > 0
            spacing: 10

            Label {
                text: app.libraryModel.selectedCount + qsTr(" selected")
                color: hasSelection ? Theme.text : Theme.textMuted
                font.pixelSize: 13
            }

            Item { Layout.fillWidth: true }

            AppButton {
                text: qsTr("Send PKG")
                enabled: page.hasPkgSelection && !app.scanning
                highlighted: page.hasPkgSelection
                onClicked: {
                    app.sendSelected()
                    app.libraryModel.clearSelection()
                }
            }

            AppButton {
                text: qsTr("Copy images")
                enabled: page.hasImageSelection && !app.scanning
                onClicked: {
                    app.copySelectedImages()
                    app.libraryModel.clearSelection()
                }
            }
        }
    }
}
