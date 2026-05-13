import QtQuick
import QtQuick.Controls
import XCPlayer

Item {
    id: root

    property LyricsModel lyricsModel: null
    property bool showLyrics: true
    property point startPos: Qt.point(0, 0)
    property point targetPos: {
        if(showLyrics) {
            return Qt.point((root.width / 2.0 - targetSize) / 2.0, (root.height - targetSize) / 2.0)
        } else {
            return Qt.point((root.width - targetSize) / 2.0, (root.height - targetSize) / 2.0)
        }
    }
    property real startSize: 48
    property real targetSize: Math.min(root.height * 0.45, root.width / 2.0 - 32)

    // 封面大图
    Image {
        source: XCPlayer.largeCover
        fillMode: Image.PreserveAspectFit
        cache: false
        x: window.coverExpanded ? root.targetPos.x : root.startPos.x
        y: window.coverExpanded ? root.targetPos.y : root.startPos.y
        width: window.coverExpanded ? root.targetSize : root.startSize
        height: window.coverExpanded ? root.targetSize : root.startSize
        opacity: (window.coverExpanded || window.isAnimating) ? 1 : 0

        Behavior on x { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }
        Behavior on y { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }
        Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }
        Behavior on height { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }

        HoverHandler {
            cursorShape: window.hideBar ? Qt.BlankCursor : Qt.PointingHandCursor
        }
        TapHandler {
            onTapped: window.coverExpanded = false
        }
    }

    // 歌词列表
    ListView {
        id: lyricsList
        visible: window.displaying && XCPlayer.lyrics.length > 0 && x < root.width
        clip: true
        x: root.showLyrics ? window.coverExpanded ? root.width / 2 : root.width / 4 : root.width
        Behavior on x { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }
        y: 80
        width: root.width / 2.0
        height: root.height - 160

        header: Item { height: lyricsList.height / 2 }
        footer: Item { height: lyricsList.height / 2 }

        spacing: 16
        model: lyricsModel

        onVisibleChanged: {
            if(visible && lyricsModel.currIndex !== -1) {
                currentIndex = lyricsModel.currIndex;
                positionViewAtIndex(currentIndex, ListView.Center);
            }
        }

        onHeightChanged: {
            if(count > 0 && currentIndex !== -1) {
                positionViewAtIndex(currentIndex, ListView.Center)
            }
        }

        // 歌词滚动动画
        NumberAnimation {
            id: lyricScrollAnim
            target: lyricsList
            property: "contentY"
            duration: 300
            easing.type: Easing.OutSine
        }

        // 歌词 Delegate
        delegate: Item {
            id: lyricDelegate
            width: ListView.view.width
            height: lyricLine.height
            opacity: model.isCurrent ? 1.0 : (hoverHandler.hovered ? 0.8 : 0.4)
            Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }

            HoverHandler { id: hoverHandler; enabled: !window.hideBar }
            TapHandler {
                enabled: window.displaying
                onTapped: {
                    // 点击歌词 Seek
                    XCPlayer.Seek(lyricsModel.GetTimeAt(index))
                    if(!XCPlayer.isPlaying)
                        XCPlayer.SetPause(false)
                }
            }

            Column {
                id: lyricLine
                width: parent.width
                anchors.centerIn: parent
                spacing: 5
                // 原文
                Text {
                    horizontalAlignment: Text.AlignHCenter
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: model.original
                    color: "white"
                    font.pointSize: model.isCurrent ? 18 : 15
                    Behavior on font.pointSize { NumberAnimation { duration: 100 } }
                }
                // 译文
                Text {
                    horizontalAlignment: Text.AlignHCenter
                    visible: text !== ""
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: model.translation
                    color: "lightgray"
                    font.pointSize: model.isCurrent ? 15 : 12
                    Behavior on font.pointSize { NumberAnimation { duration: 100 } }
                }
            }
        }
    }

    // 右侧歌词收起/展开按钮
    MyButton {
        visible: window.displaying && XCPlayer.lyrics.length > 0 && opacity > 0
        anchors.right: root.right
        anchors.verticalCenter: root.verticalCenter
        opacity: !window.hideBar ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }
        width: 40
        height: 40
        radius: 20
        icon.source:root.showLyrics ? "qrc:/assets/icons/Right.png" : "qrc:/assets/icons/Left.png"
        icon.width: 18
        icon.height: 18
        ToolTip.visible: false
        onClicked: root.showLyrics = !root.showLyrics
    }

    // 歌词同步
    Connections {
        target: XCPlayer

        // 更新歌词
        function onPlayInfoChanged() {
            lyricScrollAnim.stop()

            Qt.callLater(function() {
                lyricsModel.LoadLyrics(XCPlayer.lyrics)

                // 为了歌词滚动精确，所有歌词 delegate 一次性全部创建
                lyricsList.cacheBuffer = Math.max(0, lyricsList.contentHeight)

                lyricsList.positionViewAtIndex(0, ListView.Center)
            })

        }

        // 音频歌词同步
        function onProgressChanged() {
            // 更新当前歌词行索引
            let changed = lyricsModel.UpdateCurrentIndex(XCPlayer.masterClock)

            // 歌词列表不可见或歌词行索引无变化
            if(!lyricsList.visible || !changed) {
                return;
            }

            // 获取当前位置
            let currentY = lyricsList.contentY;

            // 更新歌词列表的当前行索引
            lyricsList.currentIndex = lyricsModel.currIndex;

            // 计算目标位置
            lyricsList.positionViewAtIndex(lyricsModel.currIndex, ListView.Center);
            let targetY = lyricsList.contentY;

            if(currentY !== targetY) {
                // 恢复当前位置
                lyricsList.contentY = currentY;

                // 执行歌词滚动动画
                lyricScrollAnim.to = targetY;
                lyricScrollAnim.restart();
            }
        }
    }
}
