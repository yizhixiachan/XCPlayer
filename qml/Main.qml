import QtQuick
import QtQuick.Layouts
import Qt.labs.platform
import QtCore
import XCPlayer

Window {
    id: window
    flags: Qt.Window | Qt.FramelessWindowHint
    width: 1080
    height: 720
    minimumWidth: 480
    minimumHeight: 480
    visible: true
    onVisibleChanged: videoDisplay.SetVisibility(window.visible)
    title: XCPlayer.playInfo.id !== -1 ? XCPlayer.playInfo.isVideo ? XCPlayer.playInfo.title
                                                                   : XCPlayer.playInfo.title + " - " + XCPlayer.playInfo.artist
    : "XC Player"
    color: "transparent"
    Component.onCompleted: {
        NativeEventFilter.SetWindow(window)
        NativeEventFilter.SetVideoDisplay(videoDisplay)
        XCPlayer.StartChecking()

        if(LaunchFile !== "") {
            XCPlayer.PlayTempUrl(LaunchFile, {})
        }
    }
    property bool displaying: false
    onDisplayingChanged: {
        if(!displaying) {
            videoDisplay.SetVisibility(false)
        } else if(XCPlayer.playInfo.isVideo) {
            videoDisplay.SetVisibility(true)
        }
    }

    property bool showSubtitle: true


    // 无操作隐藏 UI
    property bool hideBar: false
    Timer {
        id: hideBarTimer
        interval: 2500
        onTriggered: {
            if(window.displaying && ctrlBar.canHideBar && !ctrlBarHover.hovered && !titleBarHover.hovered) {
                window.hideBar = true
            }
        }
    }
    HoverHandler {
        id: globalHover
        property point lastPos: Qt.point(-1, -1)
        cursorShape: (window.displaying && window.hideBar) ? Qt.BlankCursor : Qt.ArrowCursor
        onPointChanged: {
            if(window.displaying) {
                let currentPos = globalHover.point.position
                let dx = Math.abs(currentPos.x - lastPos.x)
                let dy = Math.abs(currentPos.y - lastPos.y)

                if(lastPos.x === -1 || dx > 0.5 || dy > 0.5) {
                    window.hideBar = false
                    hideBarTimer.restart()
                }
                lastPos = currentPos
            }
        }
    }
    TapHandler {
        onPressedChanged: {
            if(window.displaying) {
                window.hideBar = false
                hideBarTimer.restart()
            }
        }
    }

    // 封面动画控制
    property bool isAnimating: false
    property bool coverExpanded: false
    onCoverExpandedChanged: {
        isAnimating = true
        animTimer.restart()
    }
    Timer {
        id: animTimer
        interval: 300
        onTriggered: isAnimating = false
    }

    // Settings
    property real volume: XCPlayer.volume       // 音量大小
    property bool floatingWindow: false         // 悬浮窗口
    property bool floatingLyrics: false         // 桌面歌词
    property real lyricsFontSize: 18            // 歌词大小
    property string backgroundColor: "#1E1E28"  // 纯色背景
    property bool coverBackground: false        // 封面背景
    property bool customBackground: false       // 自定义背景
    property string imageUrl: ""                // 自定义背景路径
    property real imageOpacity: 0.5             // 背景图片透明度
    property real blurRadius: 25.0              // 背景图片模糊强度
    property bool dynamicEffect: true           // 动态效果
    property bool exclusiveMode: false          // 独占模式
    property bool replayGain: true             // 回放增益
    onExclusiveModeChanged: XCPlayer.SetExclusiveMode(exclusiveMode)
    onReplayGainChanged: XCPlayer.SetReplayGainEnabled(replayGain)
    Settings {
        id: settings
        category: "settings"
        property alias volume: window.volume
        property alias floatingWindow: window.floatingWindow
        property alias floatingLyrics: window.floatingLyrics
        property alias lyricsFontSize: window.lyricsFontSize
        property alias backgroundColor: window.backgroundColor
        property alias coverBackground: window.coverBackground
        property alias customBackground: window.customBackground
        property alias imageUrl: window.imageUrl
        property alias imageOpacity: window.imageOpacity
        property alias blurRadius: window.blurRadius
        property alias dynamicEffect: window.dynamicEffect
        property alias replayGain: window.replayGain

        // 滤镜参数
        property alias filterBrightness: videoDisplay.brightness
        property alias filterContrast: videoDisplay.contrast
        property alias filterHue: videoDisplay.hue
        property alias filterSaturation: videoDisplay.saturation
        property alias filterNoiseReduction: videoDisplay.noiseReduction
        property alias filterEdgeEnhancement: videoDisplay.edgeEnhancement

        Component.onCompleted: {
            XCPlayer.SetVolume(volume)
            XCPlayer.SetReplayGainEnabled(replayGain)
        }
    }

    Connections {
        target: XCPlayer

        function onListIDChanged() {
            playListModel.LoadBaseInfoFromPlaylist(XCPlayer.listID)
        }

        function onPlayInfoChanged() {
            if(XCPlayer.playInfo.isVideo) {
                window.displaying = true
                window.coverExpanded = true
            }
            if(XCPlayer.playInfo.id !== -1) {
                let idx = playListProxyModel.GetIndexByID(XCPlayer.playInfo.id);
                if(idx === -1) {
                    playListModel.InsertBaseInfo(XCPlayer.playInfo.id);
                }
            }
        }

        function onBusyRequest(msg) { toastDialog.show(0, msg, -1) }
        function onProcessUrlsFinished(failedUrls, success, msg) {
            toastDialog.show(success ? 1 : 2, msg, success ? 3000 : 4000)

            if(failedUrls.length > 0) {
                failedDialog.title = "导入失败"
                failedDialog.data = failedUrls;
                failedDialog.open();
            }

            playListModel.LoadBaseInfoFromPlaylist(XCPlayer.listID)
            artistView.albumPageUpdated()
            artistView.artistPageUpdated()

            libraryView.refresh()
        }

        function onFilesMissed(missingUrls) {
            failedDialog.title = "文件丢失"
            failedDialog.data = missingUrls;
            failedDialog.open();
            playListModel.LoadBaseInfoFromPlaylist(XCPlayer.listID)
            mediaModel.LoadBaseInfoFromPlaylist(mediaView.listID)

            libraryView.refresh()
        }

        function onChunkReady(chunk, listID) {
            if(mediaView.listID === listID || mediaView.listID === 1 || mediaView.listID === 2) {
                mediaModel.AppendBaseInfo(chunk, mediaView.isVideo)
            }

            libraryView.refresh()
        }

        function onModifyFinished(mediaID, success, msg) {
            toastDialog.show(success ? 1 : 2, msg, success ? 3000 : 4000)
            if(success) {
                mediaModel.ReloadBaseInfo(mediaID)
                playListModel.ReloadBaseInfo(mediaID)

                artistView.artistPageUpdated()
                artistView.albumPageUpdated()
            }
        }

        function onReplaceFinished(success, msg) {
            toastDialog.show(success ? 1 : 2, msg, success ? 3000 : 4000)
        }

        function onCustomImageSaved(url, success, msg) {
            toastDialog.show(success ? 1 : 2, msg, success ? 3000 : 4000)
            if(success) {
                window.imageUrl = url + "?t=" + new Date().getTime()

                window.customBackground = true
            }
        }

        function onFinished(success, msg) {
            toastDialog.show(success ? 1 : 2, msg, success ? 3000 : 4000)
        }

        function onLastPositionUpdated() {
            libraryView.refresh()
        }
    }

    // --- Models ---
    // 歌单/播单
    PlaylistModel {
        id: audioPlaylistModel
        Component.onCompleted: LoadPlaylists(false)
    }
    PlaylistModel {
        id: videoPlaylistModel
        Component.onCompleted: LoadPlaylists(true)
    }

    // 媒体列表数据
    MediaModel {
        id: mediaModel
        Component.onCompleted: LoadBaseInfoFromPlaylist(1)
        onBeforeDataDeleted: (mediaIDList, listID) => {
                                 // 如果删除的是正在播放的，则寻找下一个可播放项
                                 if(listID === XCPlayer.listID && mediaIDList.includes(XCPlayer.playInfo.id)) {
                                     let count = playListProxyModel.rowCount();
                                     let currIndex = playListProxyModel.GetIndexByID(XCPlayer.playInfo.id);
                                     let nextID = -1;

                                     for(let i = 1; i < count; i++) {
                                         let idx = (currIndex + i) % count;
                                         let candidateID = playListProxyModel.GetIDByIndex(idx);

                                         if(!mediaIDList.includes(candidateID)) {
                                             nextID = candidateID;
                                             break;
                                         }
                                     }

                                     if(nextID !== -1) {
                                         XCPlayer.Play(nextID, listID);
                                     } else {
                                        XCPlayer.Reset()
                                     }
                                 }
                             }

        onAfterDataDeleted: (listID) => {
                                // 重新加载当前播放列表
                                if(XCPlayer.listID === listID) {
                                    playListModel.LoadBaseInfoFromPlaylist(XCPlayer.listID)
                                }

                                // 更新艺术家列表和专辑列表
                                if(listID === 1) {
                                    artistView.albumPageUpdated()
                                    artistView.artistPageUpdated()
                                }

                                libraryView.refresh()
                            }
    }
    SortFilterModel {
        id: mediaProxyModel
        sourceModel: mediaModel
        Component.onCompleted: {
            sortRole = MediaModel.TitleRole
            SetFilterRoles([MediaModel.TitleRole, MediaModel.ArtistRole, MediaModel.AlbumRole])
            Sort(Qt.AscendingOrder)
        }
    }

    // 当前播放列表数据
    MediaModel {
        id: playListModel
        Component.onCompleted: LoadBaseInfoFromPlaylist(1)
    }
    SortFilterModel {
        id: playListProxyModel
        sourceModel: playListModel
        Component.onCompleted: {
            sortRole = MediaModel.TitleRole
            Sort(Qt.AscendingOrder)
        }
    }

    // 歌词数据
    LyricsModel {
        id: lyricsModel
    }

    // 专辑数据
    AlbumModel {
        id: albumModel
    }


    // 背景
    Rectangle {
        id: background
        visible: !(window.displaying && XCPlayer.playInfo.isVideo)
        anchors.fill: parent
        color: "transparent"

        // 纯色背景
        Item {
            id: pureColorBG
            visible: !window.coverBackground && !window.customBackground
            anchors.fill: parent
            Rectangle { anchors.fill: parent; color: window.backgroundColor }
        }

        // 图片背景
        Item {
            id: imageBG
            visible: window.displaying ? true : (window.coverBackground || window.customBackground)
            anchors.fill: parent

            property color gradientColor1: "transparent"
            property color gradientColor2: "transparent"

            // 基础背景色
            Rectangle { anchors.fill: parent; color: window.backgroundColor }

            Image {
                id: srcImage
                width: sourceSize.width
                height: sourceSize.height
                visible: false
                source: {
                    if(window.displaying) return XCPlayer.largeCover;
                    if(window.customBackground) return window.imageUrl;
                    if(window.coverBackground) return XCPlayer.largeCover;
                    return "";
                }
                onSourceChanged: {
                    let colors
                    if(window.displaying || !window.customBackground) {
                        colors = XCPlayer.GetLargeCoverDominantColors(6);
                    } else {
                        let originalUrl = window.imageUrl.split("?")[0];
                        colors = XCPlayer.GetLocalImageDominantColors(originalUrl, 6);
                    }
                    if(colors.length >= 6) {
                        imageBG.gradientColor1 = colors[4];
                        imageBG.gradientColor2 = colors[5];
                    }
                    pass1.scheduleUpdate();
                    pass2.scheduleUpdate();
                }

                fillMode: Image.PreserveAspectCrop
            }

            // Pass1 水平模糊
            ShaderEffectSource {
                id: pass1
                live: false
                sourceItem: ShaderEffect {
                    width: srcImage.width; height: srcImage.height
                    fragmentShader: "qrc:/assets/shaders/blur.frag.qsb"
                    property variant sourceMap: srcImage
                    property vector2d offset: Qt.vector2d(1.0 / 256.0, 0.0)
                    property real radius: window.displaying ? 25.0 : window.blurRadius
                    onRadiusChanged: pass1.scheduleUpdate()
                }
            }
            // Pass2 竖直模糊
            ShaderEffectSource {
                id: pass2
                live: false
                sourceItem: ShaderEffect {
                    width: srcImage.width; height: srcImage.height
                    fragmentShader: "qrc:/assets/shaders/blur.frag.qsb"
                    property variant sourceMap: pass1
                    property vector2d offset: Qt.vector2d(0.0, 1.0 / 256.0)
                    property real radius: window.displaying ? 25.0 : window.blurRadius
                    onRadiusChanged: pass2.scheduleUpdate()
                }
            }

            // 最终背景
            ShaderEffect {
                id: pass3
                anchors.fill: parent
                fragmentShader: "qrc:/assets/shaders/beautify.frag.qsb"
                opacity: window.displaying ? 1.0 : window.coverBackground || window.customBackground ? window.imageOpacity
                                                                                                     : 0.0
                Behavior on opacity {
                    NumberAnimation {
                        duration: 300
                        easing.type: Easing.OutSine
                    }
                }

                property variant sourceMap: pass2
                property bool dynamicEffect: window.dynamicEffect
                property real time: 0.0
                property color color1: imageBG.gradientColor1
                property color color2: imageBG.gradientColor2

                UniformAnimator on time {
                    from: 0.0; to: 200 * Math.PI
                    duration: 700000; loops: Animation.Infinite
                    running: imageBG.visible && window.dynamicEffect
                }
            }
        }

    }

    // 顶部标题栏
    TitleBar {
        id: titleBar
        z: 2
        anchors.top: parent.top
        implicitWidth: parent.width
        implicitHeight: 38

        opacity: (!window.displaying || !window.hideBar) ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }


        // 拦截事件
        HoverHandler { id: titleBarHover }
        WheelHandler {}
        TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds }

        onCloseRequested: {
            window.hide()
            if(window.floatingWindow) {
                miniWindow.show()
            }
        }
    }

    // 内容区域
    Item {
        z: 1
        visible: !window.displaying
        anchors {
            top: titleBar.bottom
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        RowLayout {
            anchors.fill: parent

            NavPanel {
                id: navPanel
                Layout.preferredWidth: 180
                Layout.fillHeight: true
                audioPlaylist: audioPlaylistModel
                videoPlaylist: videoPlaylistModel
            }

            StackLayout {
                id: stackLayout
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: navPanel.pageIndex
                onCurrentIndexChanged: {
                    if(currentIndex === 0 && prevIndex === 1) {
                        mediaModel.LoadBaseInfoFromPlaylist(navPanel.listID)
                    } else if(currentIndex === 1) {
                        mediaView.clear()
                    }

                    prevIndex = currentIndex
                }

                property int prevIndex: 0

                MediaView {
                    id: mediaView
                    playlistModel: isVideo ? videoPlaylistModel : audioPlaylistModel
                    externMediaModel: mediaModel
                    externSortFilterModel: mediaProxyModel
                    listName: navPanel.listName
                    listID: navPanel.listID
                    isVideo: navPanel.isVideo

                    onListIDChanged: {
                        mediaModel.LoadBaseInfoFromPlaylist(listID)
                    }
                    onSortChanged: (listID, role, order) => {
                                       if(XCPlayer.listID === listID) {
                                           playListProxyModel.sortRole = role
                                           playListProxyModel.Sort(order)
                                       }
                                   }
                    onMediaInfoShown: (mediaID) => {
                                          XCPlayer.LoadMediaInfoAsync(mediaID)
                                          mediaInfoPopup.open()
                                      }
                }

                NetworkStreamView {
                    id: networkStreamView
                }

                ArtistView {
                    id: artistView

                }

                LibraryView {
                    id: libraryView
                }

                ToolView {

                }

                SettingView {

                }
            }
        }
    }

    // 底部控制栏
    CtrlBar {
        id: ctrlBar
        z: 2
        anchors.bottom: parent.bottom
        opacity: (!window.displaying || !window.hideBar) ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }

        externSortFilterModel: playListProxyModel
        externVideoDisplay: videoDisplay

        onXChanged: audioDisplay.startPos = getCoverBtnGlobalPos()
        onYChanged: audioDisplay.startPos = getCoverBtnGlobalPos()

        onLocatePlayingItemRequest: {
            navPanel.selectPlaylist(XCPlayer.playInfo.isVideo, XCPlayer.listID)
            mediaView.locateItem(XCPlayer.playInfo.id)
        }

        onMediaInfoShown: (mediaID) => {
                              XCPlayer.LoadMediaInfoAsync(mediaID)
                              mediaInfoPopup.open()
                          }

        // 拦截事件
        HoverHandler { id: ctrlBarHover }
        WheelHandler {}
        TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds }
    }

    AudioDisplay {
        id: audioDisplay
        visible: !XCPlayer.playInfo.isVideo && (window.displaying || window.isAnimating)
        anchors.fill: parent
        lyricsModel: lyricsModel
    }

    VideoDisplay {
        id: videoDisplay
        anchors.fill: parent
        z: 0

        WheelHandler {
            enabled: window.displaying && XCPlayer.playInfo.isVideo
            onWheel: (wheel) => {
                         if(wheel.angleDelta.y > 0) {
                             XCPlayer.SetVolume(XCPlayer.volume + 0.01)
                         } else if(wheel.angleDelta.y < 0) {
                             XCPlayer.SetVolume(XCPlayer.volume - 0.01)
                         }
                     }
        }

        TapHandler {
            enabled: window.displaying && XCPlayer.playInfo.isVideo

            onTapped: XCPlayer.SetPause(XCPlayer.isPlaying)

            onDoubleTapped: (eventPoint) => {
                if(window.visibility === Window.FullScreen) {
                    window.visibility = ctrlBar.prevVisibility
                } else {
                    ctrlBar.prevVisibility = window.visibility
                    window.visibility = Window.FullScreen
                }
            }
        }

        Text {
            id: subText
            visible: window.showSubtitle
            property real videoRatio: videoDisplay.videoWidth > 0 ? (videoDisplay.videoWidth / videoDisplay.videoHeight) : (16/9)
            property real winRatio: videoDisplay.width / Math.max(1, videoDisplay.height)
            property real actualVideoHeight: winRatio > videoRatio ? videoDisplay.height : (videoDisplay.width / videoRatio)
            property real videoYOffset: (videoDisplay.height - actualVideoHeight) / 2
            property real subtitleBottomY: videoYOffset + actualVideoHeight * 0.94

            y: subtitleBottomY - height
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width * 0.8
            // 优先显示外挂字幕
            text: XCPlayer.useExtSubtitle ? XCPlayer.extSubtitleText : videoDisplay.subtitleText
            color: "white"
            font.pointSize: Math.max(16, Math.min(44, actualVideoHeight * 0.05))
            font.bold: true
            style: Text.Outline
            styleColor: "black"
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // 系统托盘
    SystemTrayIcon {
        visible: true
        icon.source: "qrc:/assets/icons/Icon.png"
        tooltip: "XC Player"

        menu: Menu {
            MenuItem {
                text: "上一个"
                onTriggered: ctrlBar.playPrev()
            }
            MenuItem {
                text: XCPlayer.isPlaying ? "暂停" : "播放"
                onTriggered: XCPlayer.SetPause(XCPlayer.isPlaying)
            }
            MenuItem {
                text: "下一个"
                onTriggered: ctrlBar.playNext()
            }
            MenuItem {
                text: "退出"
                onTriggered: Qt.quit()
            }
        }

        // 响应双击托盘图标
        onActivated: (reason) => {
                         if(reason === SystemTrayIcon.DoubleClick) {
                             window.show()
                             window.raise()
                             window.requestActivate()
                         }
                     }
    }

    // 悬浮窗口
    FloatingWindow {
        id: miniWindow
        onPlayPrevRequest: ctrlBar.playPrev()
        onPlayNextRequest: ctrlBar.playNext()
    }
    // 桌面歌词
    FloatingLyrics {
        visible: window.floatingLyrics && !XCPlayer.playInfo.isVideo && XCPlayer.lyrics.length > 0
        lyricsModel: lyricsModel
    }

    ToastDialog {
        id: toastDialog
    }

    FailedDialog {
        id: failedDialog
    }

    // 媒体详细信息视图
    MediaInfoPopup {
        id: mediaInfoPopup
    }


    Shortcut {
        sequence: "Esc"
        onActivated: {
            if(window.visibility === Window.FullScreen) {
                window.visibility = ctrlBar.prevVisibility;
            }
        }
    }
    Shortcut {
        sequence: "Space"
        onActivated: XCPlayer.SetPause(XCPlayer.isPlaying)
    }
    Shortcut {
        sequence: "Up"
        onActivated: XCPlayer.SetVolume(XCPlayer.volume + 0.05)
    }
    Shortcut {
        sequence: "Down"
        onActivated: XCPlayer.SetVolume(XCPlayer.volume - 0.05)
    }
    Shortcut {
        sequence: "Left"
        onActivated: XCPlayer.Seek(XCPlayer.masterClock - 5.0)
    }
    Shortcut {
        sequence: "Right"
        onActivated: XCPlayer.Seek(XCPlayer.masterClock + 5.0)
    }
}

