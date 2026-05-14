import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Shapes
import QtQuick.Dialogs
import XCPlayer

Item {
    id: root
    implicitWidth: parent.width
    implicitHeight: 72

    property SortFilterModel externSortFilterModel: null
    property VideoDisplay externVideoDisplay: null
    property int prevVisibility: Window.Windowed
    property int playMode: 0    // 0: 列表循环 1: 列表随机 2: 自身循环
    property bool canHideBar: !progressSlider.pressed && !progressHover.hovered && !playlistPopup.opened &&
                              !streamListPopup.opened && !volumePopup.opened && !speedPopup.opened &&
                              !moreMenu.opened && !imageSettingsPopup.opened

    signal locatePlayingItemRequest()
    signal mediaInfoShown(int mediaID)

    Connections {
        target: XCPlayer
        function onPlayCompleted() { playNext() }

        function onPlayInfoChanged() { streamListPopup.close() }
    }

    function getCoverBtnGlobalPos() {
        return coverBtn.mapToItem(null, 0, 0)
    }

    function playMedia(id) {
        if(!externSortFilterModel) return
        XCPlayer.Play(id, XCPlayer.listID);
    }

    function playPrev() {
        if(!externSortFilterModel) return
        let count = externSortFilterModel.rowCount()
        if(count === 0) return
        let prevIndex = 0
        if(count !== 1) {
            let currIndex = externSortFilterModel.GetIndexByID(XCPlayer.playInfo.id)
            if(playMode === 0) {
                prevIndex = (currIndex - 1 + count) % count
            } else if(playMode === 1) {
                prevIndex = Math.floor(Math.random() * count)
                while(prevIndex === currIndex) prevIndex = Math.floor(Math.random() * count)
            } else {
                playMedia(XCPlayer.playInfo.id); return
            }
        }
        playMedia(externSortFilterModel.GetIDByIndex(prevIndex))
    }

    function playNext() {
        if(!externSortFilterModel) return
        let count = externSortFilterModel.rowCount()
        if(count === 0) {
            XCPlayer.SetPause(true)
            return
        }
        let nextIndex = 0
        if(count !== 1) {
            let currIndex = externSortFilterModel.GetIndexByID(XCPlayer.playInfo.id)
            if(playMode === 0) {
                nextIndex = (currIndex + 1) % count
            } else if(playMode === 1) {
                nextIndex = Math.floor(Math.random() * count)
                while(nextIndex === currIndex) nextIndex = Math.floor(Math.random() * count)
            } else {
                playMedia(XCPlayer.playInfo.id); return
            }
        }
        playMedia(externSortFilterModel.GetIDByIndex(nextIndex))
    }

    // 外挂字幕文件选择
    FileDialog {
        id: extSubFileDialog
        title: "选择字幕文件"
        nameFilters: ["字幕文件 (*.srt *.vtt *.ass)"]
        onAccepted: XCPlayer.LoadExternalSubtitleAsync(currentFile)
    }

    // 底色背景
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(30/255, 30/255, 40/255, 0.5)
        opacity: window.displaying ? 0.0 : 1.0
        Behavior on opacity {
            NumberAnimation {
                duration: 200
                easing.type: Easing.OutSine
            }
        }

    }

    // 顶部进度条
    Slider {
        id: progressSlider
        focusPolicy: Qt.NoFocus
        width: parent.width
        height: 12
        anchors.verticalCenter: parent.top
        padding: 0
        from: 0
        to: XCPlayer.isLive ? Math.max(1, XCPlayer.bufferPosition * 1000) : XCPlayer.playInfo.duration * 1000
        value: XCPlayer.masterClock * 1000
        onPressedChanged: XCPlayer.SetPause(pressed)
        onMoved: XCPlayer.Seek(value / 1000)
        handle: Item {}
        background: Item {
            // 缓冲进度
            Rectangle {
                x: progressSlider.visualPosition * progressSlider.availableWidth
                y: 5
                width: XCPlayer.isLive ? progressSlider.availableWidth - x
                                       : (XCPlayer.bufferPosition / XCPlayer.playInfo.duration) * progressSlider.availableWidth - x
                height: 2
                color: Qt.rgba(1, 1, 1, 0.2)
            }

            // 已播放进度
            Rectangle {
                y: 5
                width: progressSlider.visualPosition * progressSlider.availableWidth
                height: 2
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0.6) }
                }
            }

            // 章节分隔点
            Repeater {
                model: XCPlayer.chapters
                delegate: Item {
                    x: (modelData.startTime / XCPlayer.playInfo.duration) * progressSlider.availableWidth
                    y: 0
                    width: 2
                    height: parent.height

                    // 视觉上的断点（小竖线）
                    Rectangle {
                        anchors.centerIn: parent
                        width: 2
                        height: 6
                        radius: 1
                        color: chHover.hovered ? "#3fc1e6" : "white"
                        opacity: chHover.hovered ? 1.0 : 0.5
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }

                    HoverHandler {
                        id: chHover
                        cursorShape: Qt.PointingHandCursor
                    }

                    // 鼠标悬浮时展示章节标题
                    ToolTip {
                        visible: chHover.hovered
                        text: modelData.title
                        y: -30
                        background: Rectangle {
                            color: Qt.rgba(30/255, 35/255, 45/255, 0.95)
                            border.color: "#3fc1e6"
                            border.width: 1
                            radius: 4
                        }
                        contentItem: Text {
                            text: modelData.title
                            color: "white"
                            font.pointSize: 9
                        }
                    }
                }
            }
        }

        HoverHandler {
            id: progressHover
            cursorShape: Qt.PointingHandCursor
        }
    }

    // 底部控制栏
    Item {
        anchors {
            fill: parent
            leftMargin: 12
            rightMargin: 12
        }

        // 左对齐区域
        RowLayout {
            anchors.left: parent.left
            anchors.right: centerArea.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12

            Item {
                id: coverWrapper
                property real targetWidth: window.coverExpanded ? 0 : XCPlayer.playInfo.isVideo ? 80 : 48
                Layout.preferredWidth: targetWidth
                Layout.preferredHeight: XCPlayer.playInfo.isVideo ? 45 : 48
                Layout.alignment: Qt.AlignVCenter

                Behavior on targetWidth {
                    NumberAnimation {
                        duration: 200
                        easing.type: Easing.OutSine
                    }
                }

                MyButton {
                    id: coverBtn
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height
                    icon.source: XCPlayer.cover
                    icon.width: parent.width
                    icon.height: parent.height
                    fillMode: Image.PreserveAspectFit
                    ToolTip.visible: false
                    background: null

                    // 当展开或动画期间隐藏按钮自身
                    opacity: (window.coverExpanded || window.isAnimating) ? 0 : 1

                    HoverHandler { cursorShape: Qt.PointingHandCursor }

                    onClicked: {
                        window.displaying = true
                        window.coverExpanded = true
                    }

                    onRightClicked: root.locatePlayingItemRequest()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5
                Text {
                    Layout.fillWidth: true
                    text: XCPlayer.playInfo.title
                    color: "white"
                    font.pointSize: 11
                    elide: Text.ElideRight
                }

                Text {
                    visible: !(window.displaying || XCPlayer.playInfo.isVideo)
                    Layout.fillWidth: true
                    text: XCPlayer.playInfo.artist
                    color: "lightgray"
                    font.pointSize: 9
                    elide: Text.ElideRight
                }
                Text {
                    visible: window.displaying || XCPlayer.playInfo.isVideo
                    Layout.fillWidth: true
                    text: XCPlayer.timeText
                    color: "white"
                    font.pointSize: 9
                }
            }
        }

        // 中间区域
        Row {
            id: centerArea
            anchors.centerIn: parent
            spacing: 3

            MyButton {
                radius: 5
                icon.source: root.playMode === 0 ? "qrc:/assets/icons/RepeatAll.png" :
                                                   root.playMode === 1 ? "qrc:/assets/icons/ShuffleAll.png" : "qrc:/assets/icons/RepeatOne.png"
                icon.width: 20; icon.height: 20
                ToolTip.text: root.playMode === 0 ? "列表循环" : root.playMode === 1 ? "列表随机" : "自身循环"
                onClicked: root.playMode = (root.playMode + 1) % 3
            }

            MyButton {
                radius: 5
                icon.source: "qrc:/assets/icons/Previous.png"
                icon.width: 20; icon.height: 20
                ToolTip.text: "上一个"
                onClicked: playPrev()
            }

            MyButton {
                radius: 5
                icon.source: XCPlayer.isPlaying ? "qrc:/assets/icons/Pause.png" : "qrc:/assets/icons/Play.png"
                icon.width: 20; icon.height: 20
                ToolTip.text: XCPlayer.isPlaying ? "暂停" : "播放"
                onClicked: XCPlayer.SetPause(XCPlayer.isPlaying)
            }

            MyButton {
                radius: 5
                icon.source: "qrc:/assets/icons/Next.png"
                icon.width: 20; icon.height: 20
                ToolTip.text: "下一个"
                onClicked: playNext()
            }

            MyButton {
                id: playlistBtn
                radius: 5
                icon.source: "qrc:/assets/icons/Playlist.png"
                icon.width: 20; icon.height: 20
                ToolTip.text: "播放列表"
                onClicked: playlistPopup.open()
            }
        }

        // 右对齐区域
        RowLayout {
            anchors.left: centerArea.right
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5

            Item {
                Layout.fillWidth: true
            }

            MyButton {
                id: hwBtn
                visible: XCPlayer.playInfo.isVideo
                radius: 5
                icon.source: XCPlayer.isHWEnabled ? "qrc:/assets/icons/HW.png" : "qrc:/assets/icons/HWOff.png"
                icon.width: 26; icon.height: 26
                ToolTip.text: XCPlayer.isHWEnabled ? "关闭硬件解码" : "开启硬件解码"
                onClicked: XCPlayer.SetHWAccelEnabled(!XCPlayer.isHWEnabled)
            }

            MyButton {
                id: hdrBtn
                visible: XCPlayer.playInfo.hdrFormat.length !== 0 && XCPlayer.playInfo.hdrFormat !== "SDR"
                radius: 5
                icon.source: root.externVideoDisplay.isHDREnabled ? "qrc:/assets/icons/HDR.png" : "qrc:/assets/icons/HDROff.png"
                icon.width: 26; icon.height: 26
                ToolTip.text: root.externVideoDisplay.isHDREnabled ? "关闭HDR" : "开启HDR"
                onClicked: {
                    root.externVideoDisplay.SetHDREnabled(!root.externVideoDisplay.isHDREnabled)
                }
            }

            MyButton {
                visible: XCPlayer.audioStreams.length > 1
                radius: 5
                icon.source: "qrc:/assets/icons/AudioStream.png"
                icon.width: 26; icon.height: 26
                ToolTip.text: "音频流"
                onHoveredChanged: {
                    if(hovered) {
                        streamListPopup.streamType = streamListPopup.streamTypeEnum.Audio
                        streamListPopup.btnPos = mapToItem(Overlay.overlay, 0, 0)
                        streamListPopup.streamModel = XCPlayer.audioStreams
                        streamListPopup.open()
                    } else if(!streamListHover.hovered) {
                        streamListPopup.close()
                    }
                }
            }

            MyButton {
                visible: XCPlayer.videoStreams.length > 1
                radius: 5
                icon.source: "qrc:/assets/icons/VideoStream.png"
                icon.width: 26; icon.height: 26
                ToolTip.text: "视频流"
                onHoveredChanged: {
                    if(hovered) {
                        streamListPopup.streamType = streamListPopup.streamTypeEnum.Video
                        streamListPopup.btnPos = mapToItem(Overlay.overlay, 0, 0)
                        streamListPopup.streamModel = XCPlayer.videoStreams
                        streamListPopup.open()
                    } else if(!streamListHover.hovered) {
                        streamListPopup.close()
                    }
                }
            }

            MyButton {
                visible: XCPlayer.playInfo.isVideo
                radius: 5
                icon.source: window.showSubtitle ? "qrc:/assets/icons/SubtitleStream.png"
                                                 : "qrc:/assets/icons/SubtitleOff.png"
                icon.width: 26; icon.height: 26
                ToolTip.text: window.showSubtitle ? "关闭字幕" : "显示字幕"
                onHoveredChanged: {
                    if(hovered) {
                        streamListPopup.streamType = streamListPopup.streamTypeEnum.Subtitle
                        streamListPopup.btnPos = mapToItem(Overlay.overlay, 0, 0)

                        // 添加外挂字幕选项
                        let customModel = [];
                        customModel.push({index: -2, language: "外挂字幕"});
                        for(let i = 0; i < XCPlayer.subtitleStreams.length; i++) {
                            customModel.push(XCPlayer.subtitleStreams[i]);
                        }
                        streamListPopup.streamModel = customModel;

                        streamListPopup.open()
                    } else if(!streamListHover.hovered) {
                        streamListPopup.close()
                    }
                }
                onClicked: window.showSubtitle = !window.showSubtitle
            }

            MyButton {
                id: lyricsBtn
                visible: !XCPlayer.playInfo.isVideo && (XCPlayer.lyrics !== undefined && XCPlayer.lyrics.length > 0)
                display: AbstractButton.TextOnly
                radius: 5
                text: "词"
                textColor: window.floatingLyrics ? "#3fc1e6" : "white"
                ToolTip.text: window.floatingLyrics ? "关闭桌面歌词" : "开启桌面歌词"
                onClicked: window.floatingLyrics = !window.floatingLyrics
            }

            MyButton {
                id: volumeBtn
                Layout.alignment: Qt.AlignVCenter
                display: AbstractButton.TextUnderIcon
                radius: 5
                icon.source: XCPlayer.isMute ? "qrc:/assets/icons/Mute.png" : "qrc:/assets/icons/Volume.png"
                icon.width: 20; icon.height: 20
                text: Math.round(volumeSlider.value)
                ToolTip.text: XCPlayer.isMute ? "解除静音" : "静音"
                onHoveredChanged: {
                    if(hovered) {
                        volumePopup.open()
                    } else if(!volumePopupHover.hovered) {
                        volumePopup.close()
                    }
                }
                onClicked: XCPlayer.SetMute(!XCPlayer.isMute)

                WheelHandler {
                    target: volumeSlider
                    onWheel: (wheel) => {
                                 if(wheel.angleDelta.y > 0) {
                                     XCPlayer.SetVolume(XCPlayer.volume + 0.01)
                                 } else if(wheel.angleDelta.y < 0) {
                                     XCPlayer.SetVolume(XCPlayer.volume - 0.01)
                                 }
                             }
                }
            }

            MyButton {
                id: speedBtn
                Layout.alignment: Qt.AlignVCenter
                display: AbstractButton.TextUnderIcon
                radius: 5
                icon.source: "qrc:/assets/icons/Speed.png"
                icon.width: 20; icon.height: 20
                text: speedDial.value.toFixed(1) + "x"
                ToolTip.visible: false
                onHoveredChanged: {
                    if(hovered) {
                        speedPopup.open()
                    } else if(!speedPopupHover.hovered) {
                        speedPopup.close()
                    }
                }

                WheelHandler {
                    onWheel: (wheel) => {
                                 let delta = wheel.angleDelta.y / 120
                                 let val = Math.max(0.5, Math.min(3.0, XCPlayer.speed + delta / 10))
                                 XCPlayer.SetSpeed(val)
                             }
                }
            }

            MyButton {
                id: moreBtn
                Layout.alignment: Qt.AlignVCenter
                radius: 5
                icon.source: "qrc:/assets/icons/More.png"
                icon.width: 20; icon.height: 20
                ToolTip.visible: false
                onClicked: {
                    let h = moreMenu.height === 0 ? 56 : moreMenu.height
                    moreMenu.popup(moreBtn, (moreBtn.width - moreMenu.width) / 2, -h)
                }
                Menu {
                    id: moreMenu
                    implicitWidth: 100
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

                    MyMenuItem {
                        enabled: XCPlayer.playInfo.isVideo
                        opacity: enabled ? 1.0 : 0.3
                        height: implicitHeight
                        text: "图像设置"
                        onTriggered: imageSettingsPopup.open()
                    }

                    MyMenuItem {
                        enabled: XCPlayer.playInfo.url.length !== 0
                        opacity: enabled ? 1.0 : 0.3
                        height: implicitHeight
                        text: "媒体信息"
                        onTriggered: root.mediaInfoShown(XCPlayer.playInfo.id)
                    }
                }
            }

            MyButton {
                id: fullScreenBtn
                Layout.alignment: Qt.AlignVCenter
                radius: 5
                icon.source: window.visibility === Window.FullScreen ? "qrc:/assets/icons/ExitFullScreen.png" : "qrc:/assets/icons/FullScreen.png"
                icon.width: 20; icon.height: 20
                ToolTip.text: window.visibility === Window.FullScreen ? "退出全屏" : "全屏"
                onClicked: {
                    if (window.visibility === Window.FullScreen) {
                        window.visibility = root.prevVisibility
                    } else {
                        root.prevVisibility = window.visibility
                        window.visibility = Window.FullScreen
                    }
                }
            }
        }
    }


    // 流列表弹窗
    Popup {
        id: streamListPopup
        parent: Overlay.overlay
        height: streamModel ? Math.min(240, streamModel.length * 24) : 0
        width: maxContentWidth
        padding: 0
        closePolicy: Popup.NoAutoClose

        readonly property var streamTypeEnum: {
            "Audio": 0,
            "Video": 1,
            "Subtitle": 2
        }

        property int streamType: streamTypeEnum.Audio
        property point btnPos: Qt.point(0, 0)
        property var streamModel: null

        property real maxContentWidth: 100

        TextMetrics {
            id: textMetrics
            font.pointSize: 10
        }
        // 当流数据改变时，自动计算最长文本的宽度
        onStreamModelChanged: {
            if (!streamModel) {
                maxContentWidth = 100
                return
            }

            let maxW = 100;
            for(let i = 0; i < streamModel.length; ++i) {
                let text = streamModel[i].language.length === 0 ? "流#" + streamModel[i].index : streamModel[i].language;
                textMetrics.text = text
                maxW = Math.max(maxW, textMetrics.advanceWidth + 40)
            }
            maxContentWidth = maxW
        }

        onAboutToShow: {
            x = btnPos.x - (width - 42) / 2
            y = btnPos.y - height

            // 定位到当前流位置
            let currentStreamIndex = -1
            if(streamType === streamTypeEnum.Audio) {
                currentStreamIndex = XCPlayer.audioStreamIndex
            } else if(streamType === streamTypeEnum.Video) {
                currentStreamIndex = XCPlayer.videoStreamIndex
            } else if(streamType === streamTypeEnum.Subtitle) {
                if(XCPlayer.useExtSubtitle) {
                    currentStreamIndex = -2 // 外挂字幕的自定义 index
                } else {
                    currentStreamIndex = XCPlayer.subtitleStreamIndex
                }
                // =========================================
            }

            if(currentStreamIndex !== -1 && streamModel) {
                let targetViewIndex = -1
                for(let i = 0; i < streamModel.length; ++i) {
                    if(streamModel[i].index === currentStreamIndex) {
                        targetViewIndex = i
                        break
                    }
                }
                if(targetViewIndex !== -1) {
                    streamListView.positionViewAtIndex(targetViewIndex, ListView.Center)
                }
            }
        }

        enter: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 200
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 200
            }
        }

        HoverHandler {
            id: streamListHover
            onHoveredChanged: {
                if(!hovered) {
                    streamListPopup.close()
                }
            }
        }

        background: Rectangle {
            color: Qt.rgba(30/255, 35/255, 45/255, 0.95)
            border.color: Qt.rgba(1, 1, 1, 0.1)
            border.width: 1
            radius: 5
        }

        MyListView {
            id: streamListView
            anchors.fill: parent
            model: streamListPopup.streamModel

            delegate: MyMenuItem {
                id: menuItem
                width: streamListView.width
                height: 24

                text: modelData.language.length === 0 ?  "流#" + modelData.index : modelData.language

                // 当前选中的项
                checked: {
                    // 判断选中状态逻辑
                    if(streamListPopup.streamType === streamListPopup.streamTypeEnum.Audio) {
                        return XCPlayer.audioStreamIndex === modelData.index
                    } else if(streamListPopup.streamType === streamListPopup.streamTypeEnum.Video) {
                        return XCPlayer.videoStreamIndex === modelData.index
                    } else {
                        if(modelData.index === -2) return XCPlayer.useExtSubtitle; // 外挂字幕选中
                        return (!XCPlayer.useExtSubtitle) && (XCPlayer.subtitleStreamIndex === modelData.index)
                    }
                }

                // 切换流
                onTriggered: {
                    if(streamListPopup.streamType === streamListPopup.streamTypeEnum.Audio) {
                        XCPlayer.OpenAudioStream(modelData.index)
                    } else if(streamListPopup.streamType === streamListPopup.streamTypeEnum.Video) {
                        XCPlayer.OpenVideoStream(modelData.index)
                    } else {
                        if(modelData.index === -2) {
                            extSubFileDialog.open(); // 点击导入
                            streamListPopup.close();
                        } else {
                            XCPlayer.useExtSubtitle = false; // 关闭外挂，切回内嵌
                            XCPlayer.OpenSubtitleStream(modelData.index)
                        }
                    }
                }
            }

        }
    }

    // 播放列表弹窗
    Popup {
        id: playlistPopup
        parent: Overlay.overlay
        width: parent.width * 0.25
        height: parent.height * 0.7
        modal: true
        dim: false
        padding: 0

        onAboutToShow: {
            let btnPos = playlistBtn.mapToItem(Overlay.overlay, 0, 0)
            x = btnPos.x + playlistBtn.width
            y = btnPos.y - height

            let index = externSortFilterModel.GetIndexByID(XCPlayer.playInfo.id)
            if(index !== -1) {
                playQueueView.positionViewAtIndex(index, ListView.Center)
            }
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

        background: Rectangle {
            color: Qt.rgba(30/255, 35/255, 45/255, 0.95)
            border.color: Qt.rgba(1, 1, 1, 0.1)
            border.width: 1
            radius: 5
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                height: 40
                color: "transparent"
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 24
                    anchors.verticalCenter: parent.verticalCenter

                    text: "播放列表（" + playQueueView.count + "）"
                    color: "white"
                    font.pointSize: 11
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.1) }

            // 播放列表视图
            MyListView {
                id: playQueueView
                model: externSortFilterModel

                Layout.fillWidth: true
                Layout.fillHeight: true

                anchors.topMargin: 5
                anchors.bottomMargin: 5

                delegate: Item {
                    id: delegateItem
                    width: playQueueView.width
                    height: 48

                    readonly property bool isCurrent: model.id === XCPlayer.playInfo.id

                    HoverHandler { id: hoverHandler }
                    TapHandler {
                        id: tapHandler
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onDoubleTapped: XCPlayer.Play(model.id, XCPlayer.listID)
                    }

                    // 背景
                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: 5
                        anchors.rightMargin: 5
                        radius: 5
                        color: hoverHandler.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"

                        // 当前播放项的左侧高亮条
                        Rectangle {
                            width: 3
                            height: 30
                            radius: 2
                            color: "#3fc1e6"
                            visible: isCurrent
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 15
                        anchors.rightMargin: 15
                        spacing: 15

                        Item {
                            Layout.preferredWidth: 30
                            Layout.alignment: Qt.AlignVCenter

                            // 播放时显示控制按钮
                            MyButton {
                                visible: isCurrent
                                anchors.centerIn: parent
                                width: 30
                                height: width
                                radius: width / 2
                                icon.source: XCPlayer.isPlaying ? "qrc:/assets/icons/Pause.png" : "qrc:/assets/icons/Play.png"
                                icon.width: 16
                                icon.height: 16
                                ToolTip.visible: false
                                onClicked: XCPlayer.SetPause(XCPlayer.isPlaying)
                            }

                            // 未播放时显示序号
                            Text {
                                anchors.centerIn: parent
                                text: index + 1
                                visible: !isCurrent
                                color: "lightgray"
                                font.pointSize: 8
                            }
                        }

                        // 文本信息区域
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                Layout.fillWidth: true
                                text: model.title
                                color: isCurrent ? "#3fc1e6" : "white"
                                font.pointSize: 10
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: !model.isVideo
                                text: model.artist
                                color: isCurrent ? "#3fc1e6" : "lightgray"
                                font.pointSize: 8
                                elide: Text.ElideRight
                            }
                        }

                        // 移除按钮
                        MyButton {
                            visible: hoverHandler.hovered
                            Layout.preferredWidth: 30
                            Layout.preferredHeight: 30
                            radius: 15
                            icon.source: "qrc:/assets/icons/Close.png"
                            icon.width: 14
                            icon.height: 14
                            ToolTip.visible: false
                            onClicked: {
                                if(XCPlayer.playInfo.id === externSortFilterModel.GetIDByIndex(index)) {
                                    let count = externSortFilterModel.rowCount()
                                    let nextIndex = -1
                                    if(count !== 1) {
                                        let currIndex = externSortFilterModel.GetIndexByID(XCPlayer.playInfo.id)
                                        nextIndex = (currIndex + 1) % count
                                    }

                                    root.playMedia(externSortFilterModel.GetIDByIndex(nextIndex))
                                }
                                externSortFilterModel.removeRow(index)

                            }
                        }
                    }
                }
            }
        }
    }

    // 音量弹窗
    Popup {
        id: volumePopup
        parent: Overlay.overlay
        width: 42
        height: 100
        padding: 0
        closePolicy: Popup.NoAutoClose
        background: null

        onAboutToShow: {
            let btnPos = volumeBtn.mapToItem(Overlay.overlay, 0, 0)
            x = btnPos.x + (volumeBtn.width - width) / 2
            y = btnPos.y - height
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

        HoverHandler {
            id: volumePopupHover
            onHoveredChanged: {
                if(!hovered) {
                    volumePopup.close()
                }
            }
        }

        WheelHandler {
            onWheel: (wheel) => {
                         if(wheel.angleDelta.y > 0) {
                             XCPlayer.SetVolume(XCPlayer.volume + 0.01)
                         } else if(wheel.angleDelta.y < 0) {
                             XCPlayer.SetVolume(XCPlayer.volume - 0.01)
                         }
                     }
        }

        Timer {
            id: volumeCloseTimer
            interval: 500
            onTriggered: {
                if(!volumeBtn.hovered && !volumePopupHover.hovered) {
                    volumePopup.close()
                }
            }
        }

        // 音量条
        Slider {
            id: volumeSlider
            focusPolicy: Qt.NoFocus
            width: 12
            height: parent.height
            anchors.centerIn: parent
            orientation: Qt.Vertical
            padding: 0
            from: 0
            to: 100
            value: XCPlayer.isMute ? 0 : XCPlayer.volume * 100
            stepSize: 1

            onValueChanged: {
                volumePopup.open()
                volumeCloseTimer.restart()
            }

            onMoved: {
                XCPlayer.SetVolume(value / 100)
            }

            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }

            background: Rectangle {
                color: Qt.rgba(1, 1, 1, 0.3)
                radius: 6

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: 12
                    height: Math.max((1.0 - volumeSlider.visualPosition) * volumeSlider.height, handle.width)
                    Behavior on height {
                        NumberAnimation {
                            duration: 200
                            easing.type: Easing.OutSine
                        }
                    }
                    radius: 6
                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop { position: 0.0; color: "#3fc1e6" }
                        GradientStop { position: 1.0; color: "#4A90E2" }
                    }
                }
            }

            handle: Rectangle {
                id: handle
                x: (volumeSlider.width - width) / 2
                y: volumeSlider.visualPosition * (volumeSlider.height - height * volumeSlider.visualPosition * volumeSlider.visualPosition)
                Behavior on y {
                    NumberAnimation {
                        duration: 200
                        easing.type: Easing.OutSine
                    }
                }
                scale: volumeSlider.pressed ? 1.1 : 1.0
                Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                width: 12
                height: 12
                radius: 6
                color: "white"
            }
        }
    }

    // 倍速弹窗
    Popup {
        id: speedPopup
        parent: Overlay.overlay
        width: 80
        height: 50
        padding: 0
        closePolicy: Popup.NoAutoClose
        background: null

        onAboutToShow: {
            let btnPos = speedBtn.mapToItem(Overlay.overlay, 0, 0)
            x = btnPos.x + (speedBtn.width - width) / 2
            y = btnPos.y - height
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

        HoverHandler {
            id: speedPopupHover
            onHoveredChanged: {
                if(!hovered) {
                    speedPopup.close()
                }
            }
        }

        WheelHandler {
            onWheel: (wheel) => {
                         let delta = wheel.angleDelta.y / 120
                         let val = Math.max(0.5, Math.min(3.0, XCPlayer.speed + delta / 10))
                         XCPlayer.SetSpeed(val)
                     }
        }

        Dial {
            id: speedDial
            anchors.fill: parent
            from: 0.5
            to: 3.0
            value: XCPlayer.speed
            stepSize: 0.1
            background: null
            handle: null

            onMoved: {
                XCPlayer.SetSpeed(value)
            }

            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }
            contentItem: Item {
                // 背景圆弧
                Shape {
                    layer.enabled: true
                    layer.samples: 8
                    ShapePath {
                        strokeColor: Qt.rgba(1, 1, 1, 0.3)
                        strokeWidth: 8
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap

                        PathAngleArc {
                            centerX: 40
                            centerY: 45
                            radiusX: 40
                            radiusY: 40
                            startAngle: -150
                            sweepAngle: 120
                        }
                    }
                    // 进度圆弧
                    Shape {
                        ShapePath {
                            strokeColor: "#00ddff"
                            strokeWidth: 8
                            fillColor: "transparent"
                            capStyle: ShapePath.RoundCap

                            PathAngleArc {
                                centerX: 40
                                centerY: 45
                                radiusX: 40
                                radiusY: 40
                                startAngle: -150
                                sweepAngle: (speedDial.value - speedDial.from) / (speedDial.to - speedDial.from) * 120
                                Behavior on sweepAngle {
                                    NumberAnimation {
                                        duration: 200
                                        easing.type: Easing.OutSine
                                    }
                                }
                            }
                        }
                    }
                }

                // 指针
                Rectangle {
                    width: 2
                    height: 36
                    color: "white"
                    radius: 1
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    transformOrigin: Item.Bottom
                    rotation: (speedDial.value - speedDial.from) / (speedDial.to - speedDial.from) * 120 - 60
                    Behavior on rotation {
                        NumberAnimation {
                            duration: 200
                            easing.type: Easing.OutSine
                        }
                    }
                    scale: speedDial.pressed ? 1.1 : 1.0
                    Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                }
                Text {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 6
                    text: speedDial.value.toFixed(1) + "倍速"
                    font.pointSize: 10
                    font.bold: true
                    color: "white"
                }
            }
        }
    }

    component FilterSliderRow: RowLayout {
        id: rowRoot
        property string featureText: ""
        property bool isSupported: false

        property var backendObj: null
        property string propertyName: ""

        visible: isSupported
        spacing: 12

        Binding {
            target: sld
            property: "value"
            value: backendObj ? backendObj[propertyName] : 0.5
            restoreMode: Binding.RestoreBinding
        }

        Text {
            text: featureText
            color: "white"
            font.pointSize: 10
            font.bold: true
            style: Text.Outline
            styleColor: "black"
            Layout.preferredWidth: 60
            horizontalAlignment: Text.AlignRight
        }

        Slider {
            id: sld
            focusPolicy: Qt.NoFocus
            Layout.fillWidth: true
            Layout.preferredWidth: 160
            from: 0.0
            to: 1.0
            stepSize: 0.01
            onMoved: if(backendObj) backendObj[propertyName] = value

            HoverHandler { cursorShape: Qt.PointingHandCursor }
            WheelHandler {
                onWheel: (wheel) => {
                             if(wheel.angleDelta.y > 0) sld.increase()
                             else if(wheel.angleDelta.y < 0) sld.decrease()
                             if(backendObj) backendObj[propertyName] = sld.value
                         }
            }

            background: Rectangle {
                x: sld.leftPadding
                y: sld.topPadding + (sld.availableHeight - height) / 2
                width: sld.availableWidth
                height: 6
                radius: 3
                color: Qt.rgba(1, 1, 1, 0.2)

                Rectangle {
                    width: sld.visualPosition * parent.width
                    Behavior on width {
                        NumberAnimation {
                            duration: 200
                            easing.type: Easing.OutSine
                        }
                    }
                    height: parent.height
                    radius: 3
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "#3fc1e6" }
                        GradientStop { position: 1.0; color: "#4A90E2" }
                    }
                }
            }

            handle: Rectangle {
                x: sld.leftPadding + sld.visualPosition * (sld.availableWidth - width)
                Behavior on x { NumberAnimation {
                        duration: 200
                        easing.type: Easing.OutSine
                    }
                }
                y: sld.topPadding + (sld.availableHeight - height) / 2
                width: 16; height: 16; radius: 8
                scale: sld.pressed ? 1.1 : 1.0
                Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                color: "white"
                border.color: Qt.rgba(0, 0, 0, 0.15)
                border.width: 1
            }
        }

        Text {
            text: sld.value.toFixed(2)
            color: "white"
            font.pointSize: 10
            font.bold: true
            style: Text.Outline
            styleColor: "black"
            Layout.preferredWidth: 30
        }
    }

    // 图像设置弹窗
    Popup {
        id: imageSettingsPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 320
        height: contentCol.implicitHeight + 40
        padding: 20
        modal: true
        dim: false

        background: Rectangle {
            color: Qt.rgba(30/255, 30/255, 40/255, 0.4)
            radius: 12
            border.color: Qt.rgba(1, 1, 1, 0.2)
            border.width: 1
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutSine }
            NumberAnimation { property: "scale"; from: 0.8; to: 1.0; duration: 200; easing.type: Easing.OutBack }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 200; easing.type: Easing.InSine }
            NumberAnimation { property: "scale"; from: 1.0; to: 0.8; duration: 200; easing.type: Easing.InBack }
        }

        ColumnLayout {
            id: contentCol
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 12

            Text {
                text: "图像设置"
                color: "white"
                font.pointSize: 12
                font.bold: true
                style: Text.Outline
                styleColor: "black"
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 8
            }

            FilterSliderRow {
                featureText: "亮度"
                backendObj: root.externVideoDisplay
                propertyName: "brightness"
                isSupported: root.externVideoDisplay ? root.externVideoDisplay.brightnessSupported : false
            }
            FilterSliderRow {
                featureText: "对比度"
                backendObj: root.externVideoDisplay
                propertyName: "contrast"
                isSupported: root.externVideoDisplay ? root.externVideoDisplay.contrastSupported : false
            }
            FilterSliderRow {
                featureText: "色相"
                backendObj: root.externVideoDisplay
                propertyName: "hue"
                isSupported: root.externVideoDisplay ? root.externVideoDisplay.hueSupported : false
            }
            FilterSliderRow {
                featureText: "饱和度"
                backendObj: root.externVideoDisplay
                propertyName: "saturation"
                isSupported: root.externVideoDisplay ? root.externVideoDisplay.saturationSupported : false
            }
            FilterSliderRow {
                featureText: "降噪"
                backendObj: root.externVideoDisplay
                propertyName: "noiseReduction"
                isSupported: root.externVideoDisplay ? root.externVideoDisplay.noiseReductionSupported : false
            }
            FilterSliderRow {
                featureText: "边缘锐化"
                backendObj: root.externVideoDisplay
                propertyName: "edgeEnhancement"
                isSupported: root.externVideoDisplay ? root.externVideoDisplay.edgeEnhancementSupported : false
            }

            // 旋转控制
            RowLayout {
                spacing: 18

                Text {
                    text: "旋转"
                    color: "white"
                    font.pointSize: 10
                    font.bold: true
                    style: Text.Outline
                    styleColor: "black"
                    Layout.preferredWidth: 60
                    horizontalAlignment: Text.AlignRight
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    radius: 8
                    color: Qt.rgba(0, 0, 0, 0.2)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 4

                        // 左旋按钮
                        Button {
                            id: rotateLeftBtn
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            contentItem: Text {
                                text: "↺"
                                color: "white"
                                font.pointSize: 16
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                radius: 5
                                color: rotateLeftBtn.hovered ? Qt.rgba(63/255, 193/255, 230/255, 0.2)
                                                             : "transparent"
                                Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
                            }

                            onClicked: {
                                if(root.externVideoDisplay) {
                                    root.externVideoDisplay.rotation -= 90
                                }
                            }
                        }

                        // 角度显示
                        Text {
                            text: {
                                if(root.externVideoDisplay) {
                                    var r = root.externVideoDisplay.rotation % 360
                                    if(r < 0) r += 360
                                    return r + "°"
                                }
                                return "0°"
                            }
                            color: "white"
                            font.pointSize: 11
                            font.bold: true
                            style: Text.Outline
                            styleColor: "black"
                            horizontalAlignment: Text.AlignHCenter
                            Layout.preferredWidth: 60
                        }

                        // 右旋按钮
                        Button {
                            id: rotateRightBtn
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            contentItem: Text {
                                text: "↻"
                                color: "white"
                                font.pointSize: 16
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                radius: 5
                                color: rotateRightBtn.hovered ? Qt.rgba(63/255, 193/255, 230/255, 0.2)
                                                              : "transparent"
                                Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
                            }

                            onClicked: {
                                if(root.externVideoDisplay) {
                                    root.externVideoDisplay.rotation += 90
                                }
                            }
                        }
                    }
                }

                Item {
                    Layout.preferredWidth: 30
                }
            }

            // 重置按钮
            Button {
                id: resetBtn
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 15
                Layout.preferredWidth: 80
                Layout.preferredHeight: 30

                contentItem: Text {
                    text: "恢复默认"
                    color: "white"
                    font.pointSize: 9
                    font.bold: true
                    style: Text.Outline
                    styleColor: "black"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: 5
                    color: resetBtn.hovered ? Qt.rgba(63/255, 193/255, 230/255, 0.4)
                                            : Qt.rgba(63/255, 193/255, 230/255, 0.1)
                    Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
                    border.color: "#2B95B3"
                }

                onClicked: {
                    if(root.externVideoDisplay) {
                        root.externVideoDisplay.ResetFilters()
                        root.externVideoDisplay.rotation = 0
                    }
                }
            }
        }
    }
}
