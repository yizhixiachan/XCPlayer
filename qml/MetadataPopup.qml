import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import XCPlayer

Popup {
    id: root
    parent: Overlay.overlay
    width: parent.width * 0.5
    height: parent.height
    x: opened ? parent.width - width : parent.width
    Behavior on x { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }
    padding: 0
    modal: true
    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.4)
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }
    }

    property int currentMediaID: -1
    property bool canEdit: true
    property bool canEditLyrics: true
    property bool canReplaceCover: true

    property bool isLoading: false

    function openEditor(mediaID) {
        let url = XCPlayer.GetUrlByMediaID(mediaID)
        let caps = XCPlayer.CheckMetadataCapabilities(url)

        root.canEdit = caps.canEdit
        root.canEditLyrics = caps.canEditLyrics
        root.canReplaceCover = caps.canReplaceCover

        root.currentMediaID = mediaID
        root.isLoading = true

        coverImage.source = "qrc:/assets/icons/Audio.png"
        titleInput.text = ""
        artistInput.text = ""
        albumInput.text = ""
        lyricsInput.text = ""
        offsetInput.text = "10"

        XCPlayer.LoadMetadataAsync(mediaID)

        scrollView.ScrollBar.vertical.position = 0
        root.open()
    }

    // 歌词时间轴偏移
    function shiftLRCTime(offsetMs) {
        let text = lyricsInput.text
        if(text.trim() === "") return
        let regex = /\[(\d{2,}):(\d{2})(?:\.(\d{1,3}))?\]/g
        let minMs = -1
        let matchCount = 0
        let match
        while((match = regex.exec(text)) !== null) {
            matchCount++
            let m = match[1]
            let s = match[2]
            let msStr = match[3]
            let totalMs = parseInt(m, 10) * 60000 + parseInt(s, 10) * 1000
            if(msStr !== undefined && msStr !== "") {
                let msVal = parseInt(msStr, 10)
                if (msStr.length === 1) msVal *= 100
                else if (msStr.length === 2) msVal *= 10
                totalMs += msVal
            }
            if(minMs === -1 || totalMs < minMs) {
                minMs = totalMs
            }
        }
        if(matchCount === 0 || minMs + offsetMs < 0) return
        regex.lastIndex = 0
        let newText = text.replace(regex, function(match, m, s, msStr) {
            let totalMs = parseInt(m, 10) * 60000 + parseInt(s, 10) * 1000
            let originalPrecision = 0
            if(msStr !== undefined && msStr !== "") {
                originalPrecision = msStr.length
                let msVal = parseInt(msStr, 10)
                if (originalPrecision === 1) msVal *= 100
                else if (originalPrecision === 2) msVal *= 10
                totalMs += msVal
            }
            totalMs += offsetMs
            let newM = Math.floor(totalMs / 60000).toString().padStart(2, '0')
            let newS = Math.floor((totalMs % 60000) / 1000).toString().padStart(2, '0')
            let newMsNum = totalMs % 1000
            let targetPrecision = originalPrecision
            if(newMsNum % 10 !== 0 && targetPrecision < 3) targetPrecision = 3
            else if(newMsNum % 100 !== 0 && targetPrecision < 2) targetPrecision = 2

            let newMsFormat = ""
            if(targetPrecision === 0) return `[${newM}:${newS}]`
            else if(targetPrecision === 1) newMsFormat = Math.floor(newMsNum / 100).toString()
            else if(targetPrecision === 2) newMsFormat = Math.floor(newMsNum / 10).toString().padStart(2, '0')
            else newMsFormat = newMsNum.toString().padStart(3, '0')

            return `[${newM}:${newS}.${newMsFormat}]`
        })

        lyricsInput.text = newText
    }

    Connections {
        target: XCPlayer

        function onMetadataReady(mediaID, metadataMap, imageUrl) {
            if(mediaID !== root.currentMediaID) return
            titleInput.text = metadataMap["title"] || ""
            artistInput.text = metadataMap["artist"] || ""
            albumInput.text = metadataMap["album"] || ""
            lyricsInput.text = metadataMap["lyrics"] || ""
            coverImage.source = imageUrl
            root.isLoading = false
        }

        function onReplaceFinished(success, msg) {
            if(root.opened) {
                if(success) {
                    // 加上时间戳强制更新图片
                    coverImage.source = "image://covers/medium/" + root.currentMediaID + "?t=" + new Date().getTime()
                }
            }
        }
    }

    FileDialog {
        id: imageSelectDialog
        title: "选择图片"
        nameFilters:["图片文件 (*.png *.jpg *.jpeg *.bmp)"]
        onAccepted: XCPlayer.ReplaceCoverAsync(root.currentMediaID, currentFile);
    }

    FileDialog {
        id: coverSaveDialog
        title: "保存图片"
        fileMode: FileDialog.SaveFile
        nameFilters: ["图片文件 (*.png *.jpg *.jpeg *.bmp)"]
        defaultSuffix: "png"
        onAccepted: XCPlayer.SaveCover(root.currentMediaID, currentFile);
    }

    FileDialog {
        id: lrcSelectDialog
        title: "选择歌词文件 (.lrc)"
        nameFilters: ["歌词文件 (*.lrc)"]
        onAccepted: {
            let content = XCPlayer.ReadLRCFile(currentFile);
            if(content !== "") {
                lyricsInput.text = content;
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
            text: "正在读取元数据..."
            color: "#3fc1e6"
            font.pointSize: 12
            font.bold: true
        }
    }

    Text {
        visible: !root.canReplaceCover && !root.canEdit && !root.canEditLyrics && !root.isLoading
        anchors.centerIn: parent
        text: "该媒体文件不支持写入元数据！"
        color: "#FCA5A5"
        font.pointSize: 12
    }


    ColumnLayout {
        visible: !root.isLoading
        anchors.fill: parent

        ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            padding: 24
            bottomPadding: 0
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: ScrollBar {
                id: vScrollBar1
                implicitWidth: 10
                minimumSize: 0.03
                anchors { right: parent.right; top: parent.top; bottom: parent.bottom }

                contentItem: Rectangle {
                    radius: width / 2
                    color: Qt.rgba(1, 1, 1, 0.4)
                    opacity: vScrollBar1.active ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutSine } }
                }
            }

            ColumnLayout {
                width: scrollView.availableWidth
                height: Math.max(implicitHeight, scrollView.availableHeight)
                spacing: 12

                // 封面数据
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: root.canReplaceCover

                    Text { text: "封面图片"; color: "#3fc1e6"; font.bold: true; font.pointSize: 11 }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Image {
                            id: coverImage
                            Layout.preferredWidth: 180
                            Layout.preferredHeight: 180
                            fillMode: Image.PreserveAspectFit
                            cache: false
                        }

                        ColumnLayout {
                            Layout.alignment: Qt.AlignBottom
                            spacing: 12

                            Button {
                                Layout.preferredWidth: 100
                                Layout.preferredHeight: 32
                                background: Rectangle {
                                    color: parent.hovered ? "#3fc1e6" : "#2B95B3"
                                    Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
                                    radius: 5
                                }
                                contentItem: Text {
                                    text: "替换封面"; color: "white"
                                    font.pointSize: 9
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: imageSelectDialog.open()
                            }

                            Button {
                                Layout.preferredWidth: 100
                                Layout.preferredHeight: 32
                                background: Rectangle {
                                    color: parent.hovered ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                                    Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
                                    border.color: "gray"
                                    radius: 5
                                }
                                contentItem: Text {
                                    text: "导出封面"; color: "white"
                                    font.pointSize: 9
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: coverSaveDialog.open()
                            }
                        }
                    }
                }

                // 文本数据
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: root.canEdit

                    Text { text: "基本信息"; color: "#3fc1e6"; font.bold: true; font.pointSize: 11 }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 12
                        columnSpacing: 12

                        Text { text: "标题（title）:"; color: "gray"; font.pointSize: 10; font.bold: true; Layout.alignment: Qt.AlignRight }
                        TextField {
                            id: titleInput
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            selectedTextColor: "white"
                            selectionColor: "#0A84FF"
                            color: "white"
                            font.pointSize: 10
                            padding: 5
                            background: Rectangle {
                                color: Qt.rgba(1, 1, 1, 0.05); radius: 5
                                border.color: titleInput.activeFocus ? "#3fc1e6" : Qt.rgba(1, 1, 1, 0.1)
                            }
                        }

                        Text { text: "艺术家（artist）:"; color: "gray"; font.pointSize: 10; font.bold: true; Layout.alignment: Qt.AlignRight }
                        TextField {
                            id: artistInput
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            selectedTextColor: "white"
                            selectionColor: "#0A84FF"
                            color: "white"
                            font.pointSize: 10
                            padding: 5
                            background: Rectangle {
                                color: Qt.rgba(1, 1, 1, 0.05); radius: 5
                                border.color: artistInput.activeFocus ? "#3fc1e6" : Qt.rgba(1, 1, 1, 0.1)
                            }
                        }

                        Text { text: "专辑（album）:"; color: "gray"; font.pointSize: 10; font.bold: true; Layout.alignment: Qt.AlignRight }
                        TextField {
                            id: albumInput
                            Layout.fillWidth: true
                            Layout.preferredWidth: 0
                            selectedTextColor: "white"
                            selectionColor: "#0A84FF"
                            color: "white"
                            font.pointSize: 10
                            padding: 5
                            background: Rectangle {
                                color: Qt.rgba(1, 1, 1, 0.05); radius: 5
                                border.color: albumInput.activeFocus ? "#3fc1e6" : Qt.rgba(1, 1, 1, 0.1)
                            }
                        }
                    }
                }

                // 歌词数据
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 180
                    spacing: 12
                    visible: root.canEditLyrics

                    Text { text: "内嵌歌词"; color: "#3fc1e6"; font.bold: true; font.pointSize: 11 }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: Qt.rgba(1, 1, 1, 0.05)
                        border.color: lyricsInput.activeFocus ? "#3fc1e6" : Qt.rgba(1, 1, 1, 0.1)
                        radius: 5
                        clip: true

                        ScrollView {
                            id: lrcScrollView
                            anchors.fill: parent
                            onAvailableHeightChanged: lyricsInput.update()

                            ScrollBar.vertical: ScrollBar {
                                id: vScrollBar2
                                implicitWidth: 10
                                minimumSize: 0.03
                                anchors { right: parent.right; top: parent.top; bottom: parent.bottom }

                                contentItem: Rectangle {
                                    radius: width / 2
                                    color: Qt.rgba(1, 1, 1, 0.4)
                                    opacity: vScrollBar2.active ? 1.0 : 0.0
                                    Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutSine } }
                                }
                            }

                            TextArea {
                                id: lyricsInput
                                width: lrcScrollView.availableWidth
                                color: "white"
                                wrapMode: TextEdit.Wrap
                                font.pointSize: 10
                                placeholderText: "在此编辑歌词文本，或点击下方导入..."
                                selectedTextColor: "white"
                                selectionColor: "#0A84FF"
                                padding: 5
                                background: null
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        RowLayout {
                            spacing: 12
                            Text { text: "时间轴偏移:"; color: "white"; font.pointSize: 9 }

                            Rectangle {
                                color: Qt.rgba(1, 1, 1, 0.05)
                                border.color: Qt.rgba(1, 1, 1, 0.1)
                                radius: 5
                                Layout.preferredWidth: 120
                                Layout.preferredHeight: 28

                                RowLayout {
                                    anchors.fill: parent

                                    Button {
                                        Layout.preferredWidth: 28
                                        Layout.fillHeight: true
                                        background: Rectangle {
                                            color: parent.hovered ? Qt.rgba(1,1,1,0.1) : "transparent"
                                            Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
                                            radius: 5
                                        }
                                        contentItem: Text {
                                            text: "－"; color: "white"
                                            font.pointSize: 9
                                            font.bold: true
                                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                        }
                                        onClicked: {
                                            let val = parseInt(offsetInput.text)
                                            if(!isNaN(val) && val > 0)
                                                shiftLRCTime(-val)
                                        }
                                    }

                                    TextField {
                                        id: offsetInput
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        text: "10"
                                        color: "#3fc1e6"
                                        font.pointSize: 9
                                        font.bold: true
                                        horizontalAlignment: TextInput.AlignHCenter; verticalAlignment: TextInput.AlignVCenter
                                        validator: RegularExpressionValidator { regularExpression: /^[1-9]\d{0,4}$/ }
                                        selectedTextColor: "white"
                                        selectionColor: "#0A84FF"
                                        background: null
                                    }

                                    Button {
                                        Layout.preferredWidth: 28
                                        Layout.fillHeight: true
                                        background: Rectangle {
                                            color: parent.hovered ? Qt.rgba(1,1,1,0.1) : "transparent"
                                            Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
                                            radius: 5
                                        }
                                        contentItem: Text {
                                            text: "＋"; color: "white"
                                            font.pointSize: 9
                                            font.bold: true
                                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                        }
                                        onClicked: {
                                            let val = parseInt(offsetInput.text)
                                            if(!isNaN(val) && val > 0)
                                                shiftLRCTime(val)
                                        }
                                    }
                                }
                            }

                            Text { text: "ms"; color: "white"; font.pointSize: 9 }
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            Layout.preferredWidth: 80
                            Layout.preferredHeight: 28
                            background: Rectangle {
                                color: parent.hovered ? "#3fc1e6" : "#2B95B3"
                                Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
                                radius: 5
                            }
                            contentItem: Text {
                                text: "导入 .lrc"; color: "white"
                                font.pointSize: 9
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: lrcSelectDialog.open()
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                    visible: !root.canEditLyrics
                }
            }

        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignCenter
            Layout.bottomMargin: 12
            Layout.topMargin: 12
            spacing: 16
            Button {
                visible: root.canEdit || root.canEditLyrics
                Layout.preferredWidth: 80
                Layout.preferredHeight: 32
                background: Rectangle {
                    color: parent.hovered ? "#3fc1e6" : "#2B95B3"
                    Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
                    radius: 5
                }
                contentItem: Text {
                    text: "保存"; color: "white"
                    font.pointSize: 9
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    let newMeta = {
                        "title": titleInput.text.trim(),
                        "artist": artistInput.text.trim(),
                        "album": albumInput.text.trim(),
                        "lyrics": lyricsInput.text.trim()
                    };
                    XCPlayer.ModifyMetadataAsync(root.currentMediaID, newMeta);
                    root.close()
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
                onClicked: root.close()
            }
        }
    }
}
