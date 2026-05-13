import QtQuick
import QtQuick.Controls

Dialog {
    id: root
    anchors.centerIn: Overlay.overlay
    width: 320
    modal: true
    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.4)
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }
    }
    padding: 0
    title: "操作确认"

    property string message: ""
    property var acceptCallback: null

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


    header: Label {
        text: root.title
        color: "white"
        font.pointSize: 13
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        topPadding: 18
        bottomPadding: 10

        background: Rectangle {
            color: "transparent"
        }
    }

    contentItem: Text {
        text: root.message
        color: "white"
        font.pointSize: 11
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter

        leftPadding: 24
        rightPadding: 24
        topPadding: 10
        bottomPadding: 20
    }

    footer: DialogButtonBox {
        alignment: Qt.AlignHCenter
        spacing: 16
        bottomPadding: 20
        topPadding: 0

        background: Rectangle { color: "transparent" }

        Button {
            text: "确定"
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            implicitWidth: 100
            implicitHeight: 36

            contentItem: Text {
                text: parent.text
                color: "white"
                font.pointSize: 11
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 6
                color: parent.pressed ? "#1b629c" : (parent.hovered ? "#2b84d1" : "#2176bb")
            }
        }

        Button {
            text: "取消"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            implicitWidth: 100
            implicitHeight: 36

            contentItem: Text {
                text: parent.text
                color: parent.pressed ? "#8892a5" : (parent.hovered ? "white" : "#BEDCE6")
                font.pointSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 6
                color: parent.pressed ? "#3c485e" : (parent.hovered ? "#364257" : "transparent")
                border.color: parent.hovered ? "#5a667d" : "#465064"
                border.width: 1
            }
        }
    }

    onAccepted: {
        if (acceptCallback !== null) {
            acceptCallback()
        }
    }
}
