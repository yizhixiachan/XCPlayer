import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

AbstractButton {
    id: root

    implicitWidth: parent ? (isChild ? parent.width - 50 : parent.width) : 0
    implicitHeight: isChild ? 24 : 30
    checkable: true
    focusPolicy: Qt.NoFocus

    property bool isChild: false
    property int listID: -1
    property bool isMultiSelected: false

    signal rightClicked(int id, var btnRef, var pos)
    signal multiSelectRequested(int modifiers)
    signal normalClicked(int id)

    onClicked: {
        root.normalClicked(root.listID)
    }

    background: Rectangle {
        anchors.fill: parent
        radius: 5

        color: {
            if(root.isMultiSelected) return Qt.rgba(62/255, 80/255, 105/255, 180/255)
            if(root.checked)         return Qt.rgba(62/255, 80/255, 105/255, 1)
            if(root.hovered)         return Qt.rgba(50/255, 58/255, 75/255, 1)
            return "transparent"
        }
    }

    contentItem: Item {
        anchors.fill: parent

        Row {
            spacing: root.isChild ? 12 : 20
            anchors.verticalCenter: parent.verticalCenter

            anchors.horizontalCenter: root.isChild ? parent.horizontalCenter : undefined

            anchors.left: root.isChild ? undefined : parent.left
            anchors.leftMargin: root.isChild ? 0 : 12

            Image {
                visible: root.icon.source !== ""
                source: root.icon.source
                sourceSize: Qt.size(root.icon.width, root.icon.height)
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: root.text
                color: "white"
                font.pointSize: root.isChild ? 8 : 10
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

   MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: false

        onPressed: (mouse) => {
            // 如果是纯左键点击（没有按Ctrl也没有按Shift），穿透给 AbstractButton 以触发 normalClicked 和 checked
            if (mouse.button === Qt.LeftButton && !(mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier))) {
                mouse.accepted = false
            }
        }

        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                if(root.listID === 1 || root.listID === 2) return
                root.rightClicked(root.listID, root, Qt.point(mouse.x, mouse.y))

            } else if (mouse.button === Qt.LeftButton) {
                if(root.listID === 1 || root.listID === 2) return
                // 如果是 Shift 或 Ctrl 左键，抛出包含 modifiers 的信号给 ListView 接管
                if (mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) {
                    root.multiSelectRequested(mouse.modifiers)
                }
            }
        }
    }
}
