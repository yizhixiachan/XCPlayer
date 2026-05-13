import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root

    signal closeRequested()

    RowLayout {
        anchors.fill: parent
        spacing: 3

        MyButton {
            id: iconBtn
            visible: !window.displaying
            Layout.preferredWidth: 90
            Layout.preferredHeight: 18
            display: AbstractButton.TextBesideIcon
            icon.source: "qrc:/assets/icons/Icon.png"
            icon.width: 16
            icon.height: 16
            text: "XC Player"
            ToolTip.visible: false
            background: null

            onClicked: {
                iconMenu.popup(iconBtn, 0, iconBtn.height)
            }

            Menu {
                id: iconMenu
                implicitWidth: 80
                background: Rectangle {
                    color: Qt.rgba(40/255, 50/255, 70/255, 1)
                    border.color: Qt.rgba(70/255, 80/255, 100/255, 1)
                    border.width: 1
                    radius: 5
                }
                enter: Transition {
                    NumberAnimation {
                        property: "opacity"
                        from: 0
                        to: 1
                        duration: 300
                        easing.type: Easing.OutSine
                    }
                }

                exit: Transition {
                    NumberAnimation {
                        property: "opacity"
                        from: 1
                        to: 0
                        duration: 300
                        easing.type: Easing.OutSine
                    }
                }

                MenuItem {
                    background: Rectangle {
                        radius: 3
                        color: parent.hovered ? Qt.rgba(160/255, 255/255, 255/255, 80/255) : "transparent"
                    }
                    contentItem: Text {
                        text: "关于"
                        color: "white"
                        font.pointSize: 10
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                    }
                    onTriggered: aboutDialog.open()
                }
            }

            HoverHandler { cursorShape: Qt.PointingHandCursor }

        }

        MyButton {
            visible: window.displaying
            Layout.preferredWidth: 44
            Layout.preferredHeight: 38
            icon.source: "qrc:/assets/icons/Down.png"
            icon.width: 16
            icon.height: 16
            ToolTip.text: "收起"
            onClicked: {
                window.displaying = false;
                window.coverExpanded = false;
            }
        }

        Item { Layout.fillWidth: true }

        MyButton {
            visible: !(window.visibility === Window.FullScreen)
            Layout.preferredWidth: 44
            Layout.preferredHeight: 38
            icon.source: "qrc:/assets/icons/Minimize.png"
            icon.width: 12
            icon.height: 12
            ToolTip.text: "最小化"
            onClicked: NativeEventFilter.ShowMinimize()
        }

        MyButton {
            visible: !(window.visibility === Window.FullScreen)
            Layout.preferredWidth: 44
            Layout.preferredHeight: 38
            icon.source: window.visibility === Window.Maximized ? "qrc:/assets/icons/Restore.png"
                                                                : "qrc:/assets/icons/Maximize.png"
            icon.width: 12
            icon.height: 12
            ToolTip.text: window.visibility === Window.Maximized ? "向下还原" : "最大化"
            onClicked: window.visibility === Window.Maximized ? NativeEventFilter.ShowRestore()
                                                              : NativeEventFilter.ShowMaximize()
        }

        MyButton {
            visible: !(window.visibility === Window.FullScreen)
            Layout.preferredWidth: 44
            Layout.preferredHeight: 38
            icon.source: "qrc:/assets/icons/Close.png"
            icon.width: 12
            icon.height: 12
            ToolTip.text: "关闭"
            hoveredColor: Qt.rgba(200 / 255, 60 / 255, 60 / 255, 1.0)
            pressedColor: Qt.rgba(200 / 255, 60 / 255, 60 / 255, 160 / 255)
            onClicked: root.closeRequested()
        }
    }


    Dialog {
        id: aboutDialog
        width: 400
        padding: 0
        modal: true
        anchors.centerIn: Overlay.overlay
        title: "关于 XCPlayer"
        Overlay.modal: Rectangle {
            color: Qt.rgba(0, 0, 0, 0.4)
            Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }
        }

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

                Text {
                    text: aboutDialog.title
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
                    onClicked: aboutDialog.close()
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Qt.rgba(1, 1, 1, 0.1)
                anchors.bottom: parent.bottom
            }
        }

        contentItem: ColumnLayout {
            spacing: 0

            Item { Layout.preferredHeight: 24 }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 16
                Layout.leftMargin: 30

                Image {
                    source: "qrc:/assets/icons/Icon.png"
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 64
                    sourceSize: Qt.size(64, 64)
                }

                ColumnLayout {
                    spacing: 2

                    Text {
                        text: "XCPlayer"
                        color: "white"
                        font.pointSize: 20
                        font.bold: true
                    }

                    Text {
                        text: "版本 1.0"
                        color: "#8892a5"
                        font.pointSize: 10
                    }
                }

                Item { Layout.fillWidth: true }
            }

            Item { Layout.preferredHeight: 14 }

            Text {
                text: "基于 Qt 和 FFmpeg 构建的开源桌面媒体播放器，追求极致简洁与性能，为您提供流畅、稳定的本地音视频播放体验。"
                color: "#d1d1d1"
                font.pointSize: 10
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.leftMargin: 30
                Layout.rightMargin: 30
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.5
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 30
                Layout.rightMargin: 30
                height: 1
                color: Qt.rgba(1, 1, 1, 0.1)
            }

            Item { Layout.preferredHeight: 8 }

            Text {
                text: "作者: yizhixiachan"
                color: "#BEDCE6"
                font.pointSize: 9
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: "© 2026 yizhixiachan. All rights reserved."
                color: "#5a667d"
                font.pointSize: 8
                Layout.alignment: Qt.AlignHCenter
            }

            Item { Layout.preferredHeight: 8 }

            Button {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 160
                implicitHeight: 40
                text: "项目地址"
                onClicked: Qt.openUrlExternally("https://github.com/yizhixiachan/XCPlayer")
                background: Rectangle {
                    radius: 8
                    color: parent.pressed ? "#1b629c" : (parent.hovered ? "#2b84d1" : "#2176bb")
                }
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.pointSize: 10
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }
            }

            Item { Layout.preferredHeight: 20 }
        }

        footer: Item { height: 0 }
    }
}
