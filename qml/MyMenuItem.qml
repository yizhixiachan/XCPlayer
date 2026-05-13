import QtQuick
import QtQuick.Controls

MenuItem {
    id: root

    leftPadding: 26

    contentItem: Text {
        text: root.text
        color: "white"
        font.pointSize: 10
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: 3
        color: root.hovered ? Qt.rgba(160/255, 255/255, 255/255, 80/255) : "transparent"
    }

    indicator: Image {
        x: 5
        anchors.verticalCenter: parent.verticalCenter
        source: "qrc:/assets/icons/Check.png"
        sourceSize: Qt.size(16, 16)
        visible: root.checked
    }

    arrow: Text {
        x: root.width - width - 10
        anchors.verticalCenter: parent.verticalCenter
        visible: root.subMenu !== null
        text: "▶"
        font.pointSize: 10
        color: root.hovered ? "white" : Qt.rgba(190/255, 220/255, 230/255, 1)
    }
}
