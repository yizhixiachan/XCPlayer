import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    width: 600
    height: 400
    anchors.centerIn: Overlay.overlay
    modal: true
    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.4)
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }
    }
    padding: 0

    property var data: []

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutSine }
        NumberAnimation { property: "scale"; from: 0.8; to: 1.0; duration: 200; easing.type: Easing.OutBack }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 200; easing.type: Easing.InSine }
        NumberAnimation { property: "scale"; from: 1.0; to: 0.8; duration: 200; easing.type: Easing.InBack }
    }

    background: Rectangle {
        color: Qt.rgba(25/255, 30/255, 40/255, 0.95)
        border.color: Qt.rgba(1, 1, 1, 0.1)
        border.width: 1
        radius: 5
    }

    header: Rectangle {
        implicitHeight: 48
        color: "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 8
            spacing: 12

            Rectangle { width: 8; height: 8; radius: 4; color: "#EF4444" }

            Text {
                text: root.title + "（" + root.data.length + "）"
                color: "white"
                font.pointSize: 11
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            MyButton {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                radius: 18
                icon.source: "qrc:/assets/icons/Close.png"
                icon.width: 12
                icon.height: 12
                ToolTip.text: "关闭"
                hoveredColor: Qt.rgba(200 / 255, 60 / 255, 60 / 255, 1.0)
                pressedColor: Qt.rgba(200 / 255, 60 / 255, 60 / 255, 160 / 255)
                onClicked: root.close()
            }
        }

        Rectangle { width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.1); anchors.bottom: parent.bottom }
    }

    contentItem: Item {
        MyListView {
            id: listView
            anchors.fill: parent
            anchors.margins: 12
            anchors.rightMargin: 0
            model: root.data
            spacing: 5

            delegate: Rectangle {
                width: listView.width - 12
                height: msgText.implicitHeight + 20
                color: Qt.rgba(1, 1, 1, 0.05)
                radius: 5
                border.color: Qt.rgba(1, 1, 1, 0.1)
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 12

                    Text {
                        text: (index + 1).toString().padStart(listView.count.toString().length, '0')
                        color: "gray"
                        font.pointSize: 9
                        font.bold: true
                    }

                    TextEdit {
                        id: msgText
                        Layout.fillWidth: true
                        text: modelData
                        color: "#FCA5A5"
                        font.pointSize: 10
                        readOnly: true
                        selectionColor: "#0A84FF"
                        wrapMode: TextEdit.WrapAnywhere
                    }
                }
            }
        }
    }
}
