import QtQuick
import PS5PackageSender

Item {
    id: root

    property string name: ""
    property color color: Theme.text
    property real size: 16

    implicitWidth: size
    implicitHeight: size

    readonly property var icons: ({
        "close": { d: "M18 6L6 18M6 6l12 12" },
        "pause": { d: "M7 5h3v14H7zM14 5h3v14h-3z", filled: true },
        "play": { d: "M5 3L19 12L5 21Z", filled: true },
        "refresh": { d: "M23 4v6h-6M20.49 15a9 9 0 1 1-2.12-9.36L23 10" },
        "chevron-up": { d: "M18 15l-6-6-6 6" },
        "chevron-down": { d: "M6 9l6 6 6-6" },
        "sun": { d: "M16 12a4 4 0 1 1-8 0a4 4 0 1 1 8 0M12 2v2M12 20v2"
                     + "M4.93 4.93l1.41 1.41M17.66 17.66l1.41 1.41M2 12h2M20 12h2"
                     + "M4.93 19.07l1.41-1.41M17.66 6.34l1.41-1.41" },
        "moon": { d: "M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z" },
        "link": { d: "M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71"
                     + "M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71" }
    })

    readonly property string svg: {
        const icon = icons[name]
        if (!icon)
            return ""
        const c = color.toString()
        const filled = icon.filled === true
        return '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">'
               + '<path d="' + icon.d
               + '" fill="' + (filled ? c : "none")
               + '" stroke="' + (filled ? "none" : c)
               + '" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>'
    }

    Image {
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        source: root.svg.length > 0
                ? "data:image/svg+xml;base64," + Qt.btoa(root.svg) : ""
        sourceSize.width: Math.round(root.size * 2)
        sourceSize.height: Math.round(root.size * 2)
    }
}
