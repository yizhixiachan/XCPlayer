import QtQuick
import XCPlayer 1.0

Window {
    id: root
    transientParent: null
    minimumWidth: 120
    minimumHeight: 40
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    Component.onCompleted: {
        updateWindowSize();
        x = (Screen.width - width) / 2
        y = Screen.height - height - 300
    }

    property LyricsModel lyricsModel: null

    function updateWindowSize() {
        let targetWidth = Math.max(minimumWidth, lyricsCol.width);
        let targetHeight = Math.max(minimumHeight, lyricsCol.height);

        if(width !== targetWidth) {
            x += (width - targetWidth) / 2;
            width = targetWidth;
        }
        if(height !== targetHeight) {
            y += (height - targetHeight) / 2;
            height = targetHeight;
        }
    }

    HoverHandler { id: hoverHandler }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onPressed: root.startSystemMove()
        onWheel: (wheel) => {
                     if(wheel.angleDelta.y > 0) {
                         window.lyricsFontSize = Math.min(48, window.lyricsFontSize + 1)
                     } else if(wheel.angleDelta.y < 0) {
                         window.lyricsFontSize = Math.max(12, window.lyricsFontSize - 1)
                     }
                 }
        onDoubleClicked: {
            window.show()
            window.raise()
            window.requestActivate()
        }

    }

    Column {
        id: lyricsCol
        anchors.centerIn: parent
        spacing: 5

        property int maxTextWidth: Screen.width

        onWidthChanged: root.updateWindowSize()
        onHeightChanged: root.updateWindowSize()

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(implicitWidth, lyricsCol.maxTextWidth)
            wrapMode: Text.Wrap
            text: root.lyricsModel ? root.lyricsModel.currOriginal : ""
            font.pointSize: window.lyricsFontSize
            font.bold: true
            color: "#3fc1e6"
            style: Text.Outline
            styleColor: "black"
        }

        Text {
            visible: text !== ""
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(implicitWidth, lyricsCol.maxTextWidth)
            wrapMode: Text.Wrap
            text: root.lyricsModel ? root.lyricsModel.currTranslation : ""
            font.pointSize: window.lyricsFontSize - 4
            font.bold: true
            color: "#3fc1e6"
            style: Text.Outline
            styleColor: "black"
        }
    }
}
