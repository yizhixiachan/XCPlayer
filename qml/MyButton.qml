import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: root
    implicitWidth: 40
    implicitHeight: 40
    display: AbstractButton.IconOnly
    ToolTip.visible: hovered
    ToolTip.delay: 1500
    focusPolicy: Qt.NoFocus

    property real radius: 0
    property real fontSize: 13
    property color textColor: "white"
    property color hoveredColor: Qt.rgba(1, 1, 1, 0.08)
    property color pressedColor: Qt.rgba(1, 1, 1, 0.16)
    property int fillMode: Image.PreserveAspectFit
    property int iconHorizontalCenterOffset: 0

    signal rightClicked()


    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: root.rightClicked()
    }

    background: Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: root.pressed ? root.pressedColor
                            : root.hovered ? hoveredColor
                                           : "transparent"
    }

    contentItem: Item {
        Image {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: root.iconHorizontalCenterOffset
            width: root.icon.width
            height: root.icon.height
            source: root.icon.source
            sourceSize: Qt.size(root.icon.width, root.icon.height)
            visible: root.display === AbstractButton.IconOnly
            fillMode: root.fillMode
            cache: false
        }

        Text {
            anchors.centerIn: parent
            visible: root.display === AbstractButton.TextOnly
            text: root.text
            color: root.textColor
            font.pointSize: root.fontSize
            font.bold: true
        }

        ColumnLayout {
            anchors.centerIn: parent;
            spacing: 2
            visible: root.display === AbstractButton.TextUnderIcon

            Image {
                Layout.alignment: Qt.AlignCenter
                Layout.preferredWidth: root.icon.width
                Layout.preferredHeight: root.icon.height

                source: root.icon.source
                sourceSize: Qt.size(root.icon.width, root.icon.height)
                fillMode: root.fillMode
            }

            Text {
                Layout.alignment: Qt.AlignCenter
                text: root.text
                color: "white"
                font.pointSize: 8
            }
        }

        RowLayout {
            anchors.centerIn: parent
            spacing: 5
            visible: root.display === AbstractButton.TextBesideIcon

            Image {
                Layout.alignment: Qt.AlignCenter
                Layout.preferredWidth: root.icon.width
                Layout.preferredHeight: root.icon.height
                source: root.icon.source
                sourceSize: Qt.size(root.icon.width, root.icon.height)
                fillMode: root.fillMode
            }

            Text {
                Layout.alignment: Qt.AlignCenter
                text: root.text
                color: "white"
                font.pointSize: 8
                font.bold: true
            }
        }
    }
}
