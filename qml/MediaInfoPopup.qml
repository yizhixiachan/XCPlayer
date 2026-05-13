import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XCPlayer

Popup {
    id: root
    parent: Overlay.overlay
    width: parent.width * 0.5
    height: parent.height
    x: opened ? parent.width - width : parent.width
    Behavior on x {
        NumberAnimation { duration: 300; easing.type: Easing.OutSine }
    }
    padding: 0
    leftPadding: 12
    modal: true
    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.4)
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }
    }

    property var mediaInfoData: null
    property bool isLoading: true

    onOpened: {
        scrollView.ScrollBar.vertical.position = 0
        if (!mediaInfoData || Object.keys(mediaInfoData).length === 0) {
            isLoading = true
        }
    }

    onClosed: {
        mediaInfoData = null
        isLoading = true
    }

    Connections {
        target: XCPlayer
        function onMediaInfoReady(infoMap) {
            root.mediaInfoData = infoMap
            root.isLoading = false
        }
    }

    function formatBytes(bytes) {
        if (bytes === undefined || bytes === null || bytes === 0) return "0 字节";
        var k = 1024, sizes = ['字节', 'KB', 'MB', 'GB', 'TB'];
        var i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    function formatBitrate(bps) {
        if (bps === undefined || bps === null || bps === 0) return "未知";
        return (bps / 1000).toFixed(0) + " kbps";
    }

    // 信息行组件
    component InfoRow: RowLayout {
        property string keyText
        property string valueText

        width: parent.width - 20
        visible: valueText !== "" && valueText !== "0" && valueText !== "0.000 fps" &&
                 valueText !== "Unknown" && valueText !== "unknown" && valueText !== "未知"

        Text {
            Layout.alignment: Qt.AlignTop
            text: keyText
            color: "gray"
            font.pointSize: 10
        }
        TextEdit {
            Layout.fillWidth: true
            text: valueText
            color: "white"
            font.pointSize: 10
            readOnly: true
            selectionColor: "#0A84FF"
            wrapMode: TextEdit.Wrap
        }
    }

    // 可折叠区域组件
    component CollapsibleSection: Column {
        id: sectionRoot

        property string title
        property bool isExpanded: false
        property bool isRootSection: false
        property bool _isSelfToggling: false

        default property alias content: contentContainer.data
        width: parent.width

        Timer {
            id: toggleTimer
            interval: 250
            onTriggered: sectionRoot._isSelfToggling = false
        }

        // 标题头
        Rectangle {
            id: headerRect
            width: parent.width
            height: 32
            radius: 5
            color: hoverArea.containsMouse && !isRootSection
                   ? Qt.rgba(63/255, 193/255, 230/255, 0.08)
                   : "transparent"

            Behavior on color { ColorAnimation { duration: 150 } }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                spacing: 12

                // 折叠图标 (+/-)
                Item {
                    width: 16; height: 16
                    visible: !sectionRoot.isRootSection

                    Rectangle {
                        anchors.fill: parent
                        radius: 8
                        color: "transparent"
                        border.width: 1
                        border.color: "white"
                        Behavior on border.color { ColorAnimation { duration: 150 } }

                        Text {
                            anchors.centerIn: parent
                            text: sectionRoot.isExpanded ? "−" : "+"
                            font.pixelSize: 12
                            font.bold: true
                            color: "white"
                        }
                    }
                }

                // 标题文字
                Text {
                    Layout.fillWidth: true
                    text: sectionRoot.title
                    color: sectionRoot.isRootSection ? "#3fc1e6" : "white"
                    font.bold: sectionRoot.isRootSection
                    font.pointSize: sectionRoot.isRootSection ? 12 : 10
                }
            }

            MouseArea {
                id: hoverArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    if (!sectionRoot.isRootSection) {
                        sectionRoot._isSelfToggling = true;
                        sectionRoot.isExpanded = !sectionRoot.isExpanded;
                        toggleTimer.restart();
                    }
                }
            }
        }

        // 内容区域
        Item {
            id: expandWrapper
            width: parent.width
            height: (sectionRoot.isExpanded || sectionRoot.isRootSection) ? contentContainer.height : 0
            visible: height > 0
            clip: true

            Behavior on height {
                NumberAnimation {
                    duration: sectionRoot._isSelfToggling ? 300 : 0
                    easing.type: Easing.OutSine
                }
            }

            // 竖线指示器
            Rectangle {
                visible: !sectionRoot.isRootSection
                width: 2
                radius: 1
                color: Qt.rgba(63/255, 193/255, 230/255, 0.15)
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    left: parent.left
                    bottomMargin: 12
                    leftMargin: 18
                }
            }

            // 实际内容
            Column {
                id: contentContainer
                spacing: 5
                topPadding: 5
                anchors {
                    top: parent.top
                    left: parent.left
                    right: parent.right
                    leftMargin: sectionRoot.isRootSection ? 12 : 40
                }
            }
        }

        // 分割线
        Item {
            visible: sectionRoot.isRootSection
            width: parent.width
            height: 25

            Rectangle {
                width: parent.width - 24
                height: 1
                anchors.centerIn: parent
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.08) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }
        }
    }

    // 媒体流信息渲染器
    component StreamInfo: Column {
        property var streams: []
        property string titlePrefix

        width: parent.width

        Repeater {
            model: streams
            delegate: CollapsibleSection {
                title: titlePrefix + " #" + modelData.index + (modelData.language ? "（" + modelData.language + "）" : "")
                isExpanded: false

                // 通用属性
                InfoRow { keyText: "编码格式:"; valueText: modelData.codec || "" }
                InfoRow { keyText: "规格:"; valueText: modelData.profile || "" }
                InfoRow { keyText: "码率:"; valueText: root.formatBitrate(modelData.bitRate) }
                InfoRow { keyText: "语言:"; valueText: modelData.language || "" }
                InfoRow { keyText: "流时长:"; valueText: XCPlayer.FormatDuration(modelData.duration) }
                InfoRow { keyText: "标志:"; valueText: modelData.flags || "" }

                // 视频专属属性
                InfoRow { keyText: "分辨率:"; valueText: modelData.resolution || "" }
                InfoRow { keyText: "帧率:"; valueText: modelData.fps ? modelData.fps.toFixed(3) + " fps" : "" }
                InfoRow { keyText: "像素格式:"; valueText: modelData.pixelFormat || "" }
                InfoRow { keyText: "SAR:"; valueText: modelData.sar || "" }
                InfoRow { keyText: "DAR:"; valueText: modelData.dar || "" }
                InfoRow { keyText: "色彩范围:"; valueText: modelData.colorRange || "" }
                InfoRow { keyText: "色彩空间:"; valueText: modelData.colorSpace || "" }
                InfoRow { keyText: "传输特性:"; valueText: modelData.colorTransfer || "" }
                InfoRow { keyText: "色彩原色:"; valueText: modelData.colorPrimaries || "" }
                InfoRow { keyText: "动态范围:"; valueText: modelData.hdrFormat || "" }
                InfoRow { keyText: "旋转角度:"; valueText: (modelData.rotation !== undefined && modelData.rotation !== 0) ? modelData.rotation + "°" : "" }

                // 音频专属属性
                InfoRow { keyText: "采样率:"; valueText: modelData.sampleRate ? modelData.sampleRate + " Hz" : "" }
                InfoRow { keyText: "采样格式:"; valueText: modelData.sampleFormat || "" }
                InfoRow { keyText: "声道数:"; valueText: modelData.channels ? modelData.channels + (modelData.channelLayout ? "（" + modelData.channelLayout + "）" : "") : "" }
                InfoRow { keyText: "位深:"; valueText: modelData.bitDepth ? modelData.bitDepth + " bit" : "" }

                // 字幕专属属性
                InfoRow { keyText: "类型:"; valueText: modelData.isImageBased !== undefined ? (modelData.isImageBased ? "图片字幕" : "文本字幕") : "" }

                // 附加数据
                InfoRow { keyText: "附加数据:"; valueText: modelData.sideDataList || "" }

                // 流元数据子区域
                CollapsibleSection {
                    visible: modelData.metadata !== undefined && modelData.metadata.length > 0
                    title: "流元数据"
                    Repeater {
                        model: modelData.metadata
                        delegate: InfoRow {
                            keyText: modelData.key + ":"
                            valueText: modelData.value
                        }
                    }
                }
            }
        }
    }

    background: Item {
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(30/255, 30/255, 40/255, 0.95)
        }
        Rectangle {
            width: 2
            anchors {
                top: parent.top
                bottom: parent.bottom
                left: parent.left
            }
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.2; color: Qt.rgba(63/255, 193/255, 230/255, 0.6) }
                GradientStop { position: 0.8; color: Qt.rgba(100/255, 100/255, 255/255, 0.4) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
    }

    // 加载指示器
    Item {
        visible: root.isLoading
        anchors.centerIn: parent

        Item {
            id: loadingSpinner
            width: 36; height: 36
            anchors.horizontalCenter: parent.horizontalCenter

            Canvas {
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    ctx.strokeStyle = "rgba(255, 255, 255, 0.2)";
                    ctx.lineWidth = 4;
                    ctx.lineCap = "round";
                    ctx.beginPath();
                    ctx.arc(width/2, height/2, width/2 - 2, 0, 2 * Math.PI);
                    ctx.stroke();
                }
            }

            Canvas {
                id: fgArcLoading
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    ctx.strokeStyle = "#3fc1e6";
                    ctx.lineWidth = 3;
                    ctx.lineCap = "round";
                    ctx.beginPath();
                    var start = -Math.PI / 2;
                    ctx.arc(width/2, height/2, width/2 - 1.5, start, start + (120 * Math.PI / 180));
                    ctx.stroke();
                }
                RotationAnimator {
                    target: fgArcLoading
                    from: 0; to: 360
                    duration: 1000
                    loops: Animation.Infinite
                    running: root.isLoading && root.visible
                }
            }
        }

        Text {
            anchors.top: loadingSpinner.bottom
            anchors.topMargin: 6
            anchors.horizontalCenter: parent.horizontalCenter
            text: "正在解析媒体信息..."
            color: "#3fc1e6"
            font.pointSize: 12
            font.bold: true
        }
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        visible: !root.isLoading
        clip: true
        ScrollBar.vertical: ScrollBar {
            id: vScrollBar
            implicitWidth: 10
            minimumSize: 0.03
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom }

            contentItem: Rectangle {
                radius: width / 2
                color: Qt.rgba(1, 1, 1, 0.4)
                opacity: vScrollBar.active ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutSine } }
            }
        }

        Column {
            width: scrollView.width
            topPadding: 20
            bottomPadding: 20

            // 基础容器信息
            CollapsibleSection {
                visible: root.mediaInfoData !== null && root.mediaInfoData.url !== undefined
                title: "基础容器信息"
                isRootSection: true

                InfoRow { keyText: "文件路径:"; valueText: root.mediaInfoData ? root.mediaInfoData.url : "" }
                InfoRow { keyText: "封装格式:"; valueText: root.mediaInfoData ? root.mediaInfoData.format : "" }
                InfoRow { keyText: "文件大小:"; valueText: root.mediaInfoData ? root.formatBytes(root.mediaInfoData.fileSize) : "" }
                InfoRow { keyText: "总时长:"; valueText: root.mediaInfoData ? XCPlayer.FormatDuration(root.mediaInfoData.duration) : "" }
                InfoRow { keyText: "总码率:"; valueText: root.mediaInfoData ? root.formatBitrate(root.mediaInfoData.bitRate) : "" }
            }

            // 容器元数据
            CollapsibleSection {
                visible: root.mediaInfoData !== null && root.mediaInfoData.metadata !== undefined && root.mediaInfoData.metadata.length > 0
                title: "容器元数据"
                isRootSection: true

                Repeater {
                    model: root.mediaInfoData ? root.mediaInfoData.metadata : null
                    delegate: InfoRow {
                        keyText: modelData.key + ":"
                        valueText: modelData.value
                    }
                }
            }

            // 音频流
            CollapsibleSection {
                visible: root.mediaInfoData !== null && root.mediaInfoData.audioStreams !== undefined && root.mediaInfoData.audioStreams.length > 0
                title: "音频流（" + (root.mediaInfoData && root.mediaInfoData.audioStreams ? root.mediaInfoData.audioStreams.length : 0) + "）"
                isRootSection: true

                StreamInfo {
                    streams: root.mediaInfoData ? root.mediaInfoData.audioStreams : null
                    titlePrefix: "音频流"
                }
            }

            // 视频流
            CollapsibleSection {
                visible: root.mediaInfoData !== null && root.mediaInfoData.videoStreams !== undefined && root.mediaInfoData.videoStreams.length > 0
                title: "视频流（" + (root.mediaInfoData && root.mediaInfoData.videoStreams ? root.mediaInfoData.videoStreams.length : 0) + "）"
                isRootSection: true

                StreamInfo {
                    streams: root.mediaInfoData ? root.mediaInfoData.videoStreams : null
                    titlePrefix: "视频流"
                }
            }

            // 字幕流
            CollapsibleSection {
                visible: root.mediaInfoData !== null && root.mediaInfoData.subtitleStreams !== undefined && root.mediaInfoData.subtitleStreams.length > 0
                title: "字幕流（" + (root.mediaInfoData && root.mediaInfoData.subtitleStreams ? root.mediaInfoData.subtitleStreams.length : 0) + "）"
                isRootSection: true

                StreamInfo {
                    streams: root.mediaInfoData ? root.mediaInfoData.subtitleStreams : null
                    titlePrefix: "字幕流"
                }
            }

            // 章节信息
            CollapsibleSection {
                visible: root.mediaInfoData !== null && root.mediaInfoData.chapters !== undefined && root.mediaInfoData.chapters.length > 0
                title: "章节（" + (root.mediaInfoData && root.mediaInfoData.chapters ? root.mediaInfoData.chapters.length : 0) + "）"
                isRootSection: true

                Column {
                    width: parent.width - 24
                    spacing: 5
                    anchors {
                        left: parent.left
                        leftMargin: 12
                    }

                    Repeater {
                        model: root.mediaInfoData ? root.mediaInfoData.chapters : null
                        delegate: Rectangle {
                            width: parent.width
                            height: 38
                            color: index % 2 === 0 ? "transparent" : Qt.rgba(1, 1, 1, 0.02)
                            radius: 5

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12

                                Column {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Text {
                                        text: modelData.title || ("章节 " + (index + 1))
                                        color: "gray"
                                        font.pointSize: 9
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        text: XCPlayer.FormatDuration(modelData.startTime) + " ~ " + XCPlayer.FormatDuration(modelData.endTime)
                                        color: "white"
                                        font.pointSize: 9
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
