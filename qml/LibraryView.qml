import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import XCPlayer

Item {
    id: root

    property var stats: []

    function refresh() {
        stats = XCPlayer.GetLibraryStats()
    }

    Component.onCompleted: refresh()

    FileDialog {
        id: fileDialog
        title: "选择文件"
        fileMode: FileDialog.OpenFiles
        nameFilters:[
            "媒体文件 (*.mp3 *.aac *.m4a *.flac *.wav *.ape *.ogg *.oga *.opus *.ac3 *.dts *.amr *.mp4 *.mkv *.webm *.avi *.mov *.flv *.ogv *.ts *.m2ts *.vob *.mpeg *.mpg)",
            "音频文件 (*.mp3 *.aac *.m4a *.flac *.wav *.ape *.ogg *.oga *.opus *.ac3 *.dts *.amr)",
            "视频文件 (*.mp4 *.mkv *.webm *.avi *.mov *.flv *.ogv *.ts *.m2ts *.vob *.mpeg *.mpg)",
            "所有文件 (*.*)"
        ]
        onAccepted: {
            XCPlayer.ProcessUrls(fileDialog.currentFiles, 1)
        }
    }

    FolderDialog {
        id: folderDialog
        title: "选择文件夹"
        onAccepted: {
            XCPlayer.ProcessUrls([folderDialog.currentFolder] , 1)
        }
    }

    ColumnLayout {
        anchors.fill: parent


        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 5

            Text {
                text: "媒体库"
                color: "white"
                font.pointSize: 14
            }

            Item { Layout.fillWidth: true }

            MyButton {
                radius: 5
                ToolTip.text: "导入文件"
                icon.source: "qrc:/assets/icons/FileScan.png"
                icon.width: 20
                icon.height: 20
                onClicked: fileDialog.open()
            }

            MyButton {
                radius: 5
                ToolTip.text: "导入文件夹"
                icon.source: "qrc:/assets/icons/FolderScan.png"
                icon.width: 20
                icon.height: 20
                onClicked: folderDialog.open()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            Layout.topMargin: 0
            Layout.bottomMargin: 12
            Layout.rightMargin: 30
            Layout.leftMargin: 30
            radius: 10
            color: "transparent"
            border.color: Qt.rgba(1, 1, 1, 0.2)
            border.width: 1

            DropArea {
                id: dropArea
                anchors.fill: parent
                onEntered: (drag) => {
                               hasUrls = drag.hasUrls
                           }
                onExited: hasUrls = false
                onDropped: (drop) => {
                               hasUrls = false
                               if(drop.hasUrls) {
                                   XCPlayer.ProcessUrls(drop.urls, 1)
                               } else {
                                   drop.accepted = false
                               }
                           }
                property bool hasUrls: false

                // Drag 背景
                Rectangle {
                    anchors.fill: parent
                    color: Qt.rgba(120/255, 200/255, 240/255, 30/255)
                    radius: 10
                    visible: dropArea.containsDrag && dropArea.hasUrls
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 10

                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: "qrc:/assets/icons/Drop.png"
                    sourceSize: Qt.size(20, 20)
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "拖入文件或文件夹将自动导入"
                    color: "lightgray"
                    font.pointSize: 9
                }
            }
        }

        Rectangle {
            id: statsPanel
            Layout.fillWidth: true
            Layout.preferredHeight: 140
            Layout.topMargin: 0
            Layout.bottomMargin: 12
            Layout.rightMargin: 30
            Layout.leftMargin: 30
            radius: 10

            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.07) }
                GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0.03) }
            }
            border.color: Qt.rgba(1, 1, 1, 0.1)
            border.width: 1

            GridLayout {
                anchors.fill: parent
                anchors.margins: 10
                columns: 3

                Repeater {
                    model:[
                        {val: root.stats.audioCount || 0, lab: "音乐"},
                        {val: root.stats.artistCount || 0, lab: "艺术家"},
                        {val: root.stats.albumCount || 0, lab: "专辑"},
                        {val: root.stats.videoCount || 0, lab: "视频"},
                        {val: root.stats.watchedVideoCount || 0, lab: "已看完"},
                        {val: root.stats.hdrVideoCount || 0, lab: "HDR"}
                    ]

                    StatItem {
                        value: modelData.val
                        label: modelData.lab
                        Layout.preferredWidth: statsPanel.width / 3
                        Layout.fillHeight: true
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }

    component StatItem: Column {
        property string label: ""
        property int value: 0

        spacing: 3
        width: parent.width

        Text {
            text: parent.value
            color: "white"
            font.pointSize: 14
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
        }

        Text {
            text: parent.label
            color: "lightgray"
            font.pointSize: 10
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
        }
    }
}
