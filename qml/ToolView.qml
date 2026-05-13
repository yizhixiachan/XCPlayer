import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import XCPlayer

Item {
    id: root

    FileDialog {
        id: lrcFileDialog
        title: "选择LRC歌词文件(可多选)"
        nameFilters:["LRC歌词 (*.lrc)"]
        fileMode: FileDialog.OpenFiles
        onAccepted: {
            let matches = XCPlayer.MatchLRCFiles(currentFiles)
            if(matches.length > 0) {
                matchModel.clear()
                for(let i=0; i<matches.length; i++) {
                    matchModel.append(matches[i])
                }
                matchDialog.open()
            } else {
                toastDialog.show(2, "未找到匹配的音乐数据！", 4000)
            }
        }
    }

    Dialog {
        id: matchDialog
        width: parent.width * 0.8
        height: parent.height * 0.8
        anchors.centerIn: Overlay.overlay
        padding: 0
        modal: true
        Overlay.modal: Rectangle {
            color: Qt.rgba(0, 0, 0, 0.5)
            Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutSine }
            NumberAnimation { property: "scale"; from: 0.9; to: 1.0; duration: 200; easing.type: Easing.OutBack }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 200; easing.type: Easing.InSine }
            NumberAnimation { property: "scale"; from: 1.0; to: 0.9; duration: 200; easing.type: Easing.InBack }
        }

        background: Rectangle {
            color: Qt.rgba(25/255, 30/255, 40/255, 0.95)
            border.color: Qt.rgba(1, 1, 1, 0.1)
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

                Rectangle { width: 8; height: 8; radius: 4; color: "#0A84FF" }

                Text {
                    text: "匹配结果（" + matchModel.count + "）"
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
                    onClicked: matchDialog.close()
                }
            }

            Rectangle { width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.1); anchors.bottom: parent.bottom }
        }

        ListModel { id: matchModel }

        contentItem: Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                // 全选
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 46
                    color: Qt.rgba(1, 1, 1, 0.04)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    radius: 5

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16

                        CheckBox {
                            id: selectAllBox
                            checked: true
                            Layout.alignment: Qt.AlignVCenter

                            onClicked: {
                                for(let i = 0; i < matchModel.count; i++) {
                                    matchModel.setProperty(i, "selected", checked)
                                }
                            }

                            indicator: Rectangle {
                                implicitWidth: 20; implicitHeight: 20; radius: 3
                                y: (parent.height - height) / 2
                                color: selectAllBox.checked ? "#0A84FF" : "transparent"
                                border.color: selectAllBox.checked ? "#0A84FF" : Qt.rgba(1, 1, 1, 0.3)
                                Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }

                                Image {
                                    anchors.centerIn: parent
                                    width: 16; height: 16
                                    source: "qrc:/assets/icons/Check.png"
                                    visible: selectAllBox.checked
                                    fillMode: Image.PreserveAspectCrop
                                }
                            }

                            contentItem: Text {
                                text: "全选"
                                color: "white"
                                font.pointSize: 10
                                font.bold: true
                                verticalAlignment: Text.AlignVCenter
                                y: (parent.height - height) / 2
                                leftPadding: selectAllBox.indicator.width + 12
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }

                // 匹配项列表
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Qt.rgba(1, 1, 1, 0.02)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                    radius: 8
                    clip: true

                    ScrollView {
                        id: dialogScrollView
                        anchors.fill: parent
                        anchors.margins: 8

                        ScrollBar.vertical: ScrollBar {
                            id: vDialogScrollBar
                            implicitWidth: 8; minimumSize : 0.05
                            contentItem: Rectangle {
                                radius: 4; color: Qt.rgba(1, 1, 1, 0.3)
                                opacity: vDialogScrollBar.active ? 1.0 : 0.0
                                Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }
                            }
                        }

                        ListView {
                            id: matchListView
                            anchors.fill: parent
                            model: matchModel
                            spacing: 4
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: Rectangle {
                                width: matchListView.width
                                implicitHeight: 56
                                radius: 8
                                color: itemHover.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent"
                                border.color: itemHover.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent"
                                border.width: 1
                                Behavior on color { ColorAnimation { duration: 150 } }

                                HoverHandler { id: itemHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: model.selected = !model.selected }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 16; anchors.rightMargin: 16
                                    spacing: 14

                                    CheckBox {
                                        id: itemCheck
                                        checked: model.selected
                                        Layout.alignment: Qt.AlignVCenter
                                        onClicked: model.selected = checked
                                        focusPolicy: Qt.NoFocus
                                        indicator: Rectangle {
                                            implicitWidth: 20; implicitHeight: 20; radius: 3
                                            y: (parent.height - height) / 2
                                            color: itemCheck.checked ? "#0A84FF" : "transparent"
                                            Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
                                            border.color: itemCheck.checked ? "#0A84FF" : Qt.rgba(1, 1, 1, 0.3)

                                            Image {
                                                anchors.centerIn: parent
                                                width: 16; height: 16
                                                source: "qrc:/assets/icons/Check.png"
                                                visible: itemCheck.checked
                                                fillMode: Image.PreserveAspectCrop
                                            }
                                        }
                                    }

                                    Rectangle {
                                        Layout.alignment: Qt.AlignVCenter
                                        width: 32; height: 32; radius: 16
                                        color: Qt.rgba(1, 1, 1, 0.08)
                                        Text { anchors.centerIn: parent; text: "🎵"; font.pointSize: 12 }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignVCenter
                                        spacing: 4

                                        Text {
                                            text: model.title + "  ·  " + model.artist
                                            color: "white"
                                            font.bold: true
                                            font.pointSize: 10
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        RowLayout {
                                            spacing: 6
                                            Rectangle {
                                                implicitWidth: srcText.width + 10; implicitHeight: 16; radius: 4
                                                color: Qt.rgba(63/255, 193/255, 230/255, 0.15)
                                                Text { id: srcText; anchors.centerIn: parent; text: "LRC"; color: "#3fc1e6"; font.pointSize: 8; font.bold: true }
                                            }
                                            Text {
                                                text: model.lrcName
                                                color: Qt.rgba(1, 1, 1, 0.4)
                                                font.pointSize: 8
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignCenter
                    Layout.topMargin: 4
                    Layout.bottomMargin: 4
                    spacing: 16

                    Button {
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 32
                        background: Rectangle {
                            color: parent.hovered ? "#3fc1e6" : "#2B95B3"
                            Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
                            radius: 5
                        }
                        contentItem: Text {
                            text: "导入"
                            color: "white"
                            font.pointSize: 9
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            let dataList = []
                            for(let i=0; i<matchModel.count; i++) {
                                let item = matchModel.get(i)
                                dataList.push({
                                                  "id": item.id,
                                                  "lrcUrl": item.lrcUrl,
                                                  "selected": item.selected
                                              })
                            }
                            XCPlayer.WriteLRCFiles(dataList)
                            matchDialog.close()
                        }
                    }

                    Button {
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 32
                        background: Rectangle {
                            color: parent.hovered ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                            Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
                            border.color: "gray"
                            radius: 5
                        }
                        contentItem: Text {
                            text: "取消"; color: "white"
                            font.pointSize: 9
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: matchDialog.close()
                    }
                }
            }
        }
    }

    component SettingsCard: Rectangle {
        Layout.fillWidth: true
        implicitHeight: cardLayout.implicitHeight + 8
        color: Qt.rgba(1, 1, 1, 0.04)
        radius: 14
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
        default property alias content: cardLayout.data
        ColumnLayout { id: cardLayout; anchors.fill: parent; anchors.margins: 4; spacing: 2 }
    }

    component ActionRow: Rectangle {
        id: actionRowRoot
        property string iconText: "🔧"
        property string iconBgColor: "#0A84FF"
        property string titleText: ""
        property string descText: ""
        signal clicked()

        Layout.fillWidth: true
        implicitHeight: 76
        color: rowHover.hovered ? Qt.rgba(1, 1, 1, 0.03) : "transparent"
        radius: 10
        Behavior on color { ColorAnimation { duration: 150 } }

        HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: actionRowRoot.clicked() }

        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 20
            spacing: 16

            Rectangle {
                width: 44; height: 44; radius: 22; color: actionRowRoot.iconBgColor
                Text { anchors.centerIn: parent; text: actionRowRoot.iconText; color: "white"; font.bold: true; font.pointSize: 11 }
            }

            ColumnLayout {
                Layout.fillWidth: true; spacing: 4
                Text { text: actionRowRoot.titleText; color: "white"; font.pointSize: 11; font.bold: true}
                Text {
                    Layout.fillWidth: true; wrapMode: Text.Wrap
                    text: actionRowRoot.descText; font.pointSize: 9; color: Qt.rgba(1, 1, 1, 0.5)
                }
            }
        }
    }

    ScrollView {
        id: mainScrollView
        anchors.fill: parent
        padding: 24

        ScrollBar.vertical: ScrollBar {
            id: vMainScrollBar
            implicitWidth: 10; minimumSize : 0.03; bottomPadding: 80
            contentItem: Rectangle {
                radius: width / 2; color: Qt.rgba(1, 1, 1, 0.4)
                opacity: vMainScrollBar.active ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutSine } }
            }
        }

        ColumnLayout {
            width: mainScrollView.availableWidth
            spacing: 24

            ColumnLayout {
                Layout.fillWidth: true; spacing: 10
                Text { text: "歌词工具"; color: "white"; font.pointSize: 15; font.bold: true }

                SettingsCard {
                    ActionRow {
                        iconText: "LRC"
                        iconBgColor: "#0A84FF"
                        titleText: "批量导入本地 LRC 歌词"
                        descText: "选择一个或多个 LRC 文件，系统将通过文件名智能匹配媒体库中的音乐。"
                        onClicked: lrcFileDialog.open()
                    }
                }
            }

            Item { height: 80 }
        }
    }
}
