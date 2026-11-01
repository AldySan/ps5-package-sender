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
        nameFilters: [qsTr("Packages and images (*.pkg *.exfat *.ffpkg *.ffpfs *.ffpfsc)")]
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

            Button {
                text: qsTr("+ Add folder")
                enabled: !app.scanning
                onClicked: folderDialog.open()
            }

            Button {
                text: qsTr("+ Add PKG files")
                enabled: !app.scanning
                onClicked: pkgDialog.open()
            }

            Button {
                text: qsTr("Rescan")
                enabled: !app.scanning && app.libraryModel.count > 0
                onClicked: app.rescanAll()
            }

            TextField {
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
                        color: active ? Theme.accentSoft : Theme.chip
                        border.width: active ? 1 : 0
                        border.color: Theme.accent
                    }

                    contentItem: Label {
                        text: parent.text
                        color: parent.active ? Theme.accent : Theme.textMuted
                        font: parent.font
                    }
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                visible: app.libraryModel.familyFilter.length > 0
                flat: true
                text: qsTr("↩ Back to all")
                font.pixelSize: 12
                onClicked: app.libraryModel.familyFilter = ""
            }

            Button {
                text: qsTr("Select all")
                flat: true
                font.pixelSize: 12
                onClicked: app.libraryModel.selectVisible(true)
            }

            Button {
                text: qsTr("Clear")
                flat: true
                font.pixelSize: 12
                enabled: page.hasSelection
                onClicked: app.libraryModel.clearSelection()
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

            delegate: GameCard {
                width: grid.cellWidth - 8
                height: grid.cellHeight - 8
                anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            }
        }

        Label {
            visible: app.libraryModel.count === 0
            Layout.fillHeight: true
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: app.scanning ? qsTr("Scanning…")
                 : qsTr("No packages yet — add a folder or drop PKG files here")
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

            Button {
                text: qsTr("Send PKG")
                enabled: page.hasPkgSelection && !app.scanning
                highlighted: page.hasPkgSelection
                onClicked: {
                    app.sendSelected()
                    app.libraryModel.clearSelection()
                }
            }

            Button {
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
