import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XCPlayer 1.0

Window {
    id: root
    transientParent: null
    width: 180
    height: 90
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    Component.onCompleted: {
        x = Screen.width - width
        y = (Screen.height - height)/ 2
    }

    signal playPrevRequest()
    signal playNextRequest()


    HoverHandler {
        id: hoverHandler
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onPressed: root.startSystemMove()

        onWheel: (wheel) => {
                     if(wheel.angleDelta.y > 0) {
                         XCPlayer.SetVolume(XCPlayer.volume + 0.05)
                     } else if(wheel.angleDelta.y < 0) {
                         XCPlayer.SetVolume(XCPlayer.volume - 0.05)
                     }
                 }

        onDoubleClicked: {
            window.show()
            window.raise()
            window.requestActivate()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 3
        spacing: 0

        RowLayout {
            spacing: 10
            Image {
                id: coverImg
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                source: XCPlayer.cover
                sourceSize: Qt.size(48, 48)
                fillMode: Image.PreserveAspectFit
                cache: false
            }
            ColumnLayout {
                spacing: 3
                Text {
                    Layout.fillWidth: true
                    text: XCPlayer.playInfo.title
                    color: "white"
                    style: Text.Outline
                    styleColor: "black"
                    font.pointSize: 11
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: XCPlayer.timeText
                    color: "white"
                    style: Text.Outline
                    styleColor: "black"
                    font.pointSize: 9
                    elide: Text.ElideRight
                }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 5
            opacity: hoverHandler.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }

            MyButton {
                Layout.preferredWidth: 28; Layout.preferredHeight: 28
                icon.source: "qrc:/assets/icons/Previous.png"
                icon.width: 20; icon.height: 20
                radius: 3
                ToolTip.visible: false
                onClicked: root.playPrevRequest()
            }
            MyButton {
                Layout.preferredWidth: 28; Layout.preferredHeight: 28
                icon.source: XCPlayer.isPlaying ? "qrc:/assets/icons/Pause.png" : "qrc:/assets/icons/Play.png"
                icon.width: 20; icon.height: 20
                radius: 3
                ToolTip.visible: false
                onClicked: XCPlayer.SetPause(XCPlayer.isPlaying)
            }
            MyButton {
                Layout.preferredWidth: 28; Layout.preferredHeight: 28
                icon.source: "qrc:/assets/icons/Next.png"
                icon.width: 20; icon.height: 20
                radius: 3
                ToolTip.visible: false
                onClicked: root.playNextRequest()
            }

        }
    }
}
