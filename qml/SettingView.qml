import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: root

    // 拾色器弹窗
    Popup {
        id: colorPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 300
        height: popupLayout.implicitHeight + 40
        padding: 5
        modal: true
        dim: false
        background: Rectangle {
            color: Qt.rgba(30/255, 30/255, 40/255, 0.95)
            radius: 12
            border.color: Qt.rgba(1, 1, 1, 0.2)
            border.width: 1
        }
        onOpened: syncFromColor(window.backgroundColor)
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutSine }
            NumberAnimation { property: "scale"; from: 0.8; to: 1.0; duration: 200; easing.type: Easing.OutBack }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 200; easing.type: Easing.InSine }
            NumberAnimation { property: "scale"; from: 1.0; to: 0.8; duration: 200; easing.type: Easing.InBack }
        }

        property real hue: 0.0
        property real sat: 0.0
        property real val: 0.0
        property real alpha: 1.0
        property bool isUpdating: false
        property color currColor: Qt.hsva(hue, sat, val, alpha)

        function syncFromColor(c) {
            if(isUpdating) return;
            isUpdating = true;
            let qcolor = Qt.color(c);
            if(!isNaN(qcolor.hsvHue)) hue = qcolor.hsvHue;
            sat = qcolor.hsvSaturation;
            val = qcolor.hsvValue;
            alpha = qcolor.a;
            isUpdating = false;
        }

        function updateBackgroundColor() {
            if(isUpdating) return;
            isUpdating = true;
            window.backgroundColor = currColor.toString();
            isUpdating = false;
        }

        function getHsl() {
            let l = val * (1 - sat / 2);
            let s = (l === 0 || l === 1) ? 0 : (val - l) / Math.min(l, 1 - l);
            return { h: hue, s: s, l: l };
        }

        function setFromHsl(h, s, l) {
            let v = l + s * Math.min(l, 1 - l);
            let sv = (v === 0) ? 0 : 2 * (1 - l / v);
            if (!isUpdating) {
                hue = h; sat = sv; val = v;
                updateBackgroundColor();
            }
        }

        // 透明度棋盘格背景
        component Checkerboard:  Canvas {
            property int radius: 8
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.beginPath();
                ctx.roundedRect(0, 0, width, height, radius, radius);
                ctx.clip();

                var gridSize = 6;
                var cols = Math.ceil(width / gridSize);
                var rows = Math.ceil(height / gridSize);

                for(var i = 0; i < cols; i++) {
                    for(var j = 0; j < rows; j++) {
                        ctx.fillStyle = ((i + j) % 2 === 0) ? "#2A2A35" : "#1D1D25";
                        ctx.fillRect(i * gridSize, j * gridSize, gridSize, gridSize);
                    }
                }
            }
        }

        // 颜色输入框
        component ChannelInput: Rectangle {
            property string valueText: ""
            property string label: ""
            property bool isHex: false
            signal valueEdited(string text)

            Layout.fillWidth: true; height: 36
            color: "transparent"
            radius: 8
            border.color: input.activeFocus ? "#3fc1e6" : Qt.rgba(1, 1, 1, 0.1); border.width: 1
            Behavior on border.color { ColorAnimation { duration: 200 } }

            TextInput {
                id: input
                anchors.fill: parent
                horizontalAlignment: TextInput.AlignHCenter; verticalAlignment: TextInput.AlignVCenter
                color: input.activeFocus ? "#3fc1e6" : "white"
                Behavior on color { ColorAnimation { duration: 200 } }
                font.pointSize: 10; selectionColor: "#0A84FF"
                maximumLength: parent.isHex ? 9 : 3
                validator: RegularExpressionValidator { regularExpression: parent.isHex ? /^#?[0-9A-Fa-f]*$/ : /^[0-9]*$/  }
                text: parent.valueText
                onEditingFinished: {
                    parent.valueEdited(text)
                    input.text = Qt.binding(function() { return parent.valueText })
                }
            }
            Text {
                text: parent.label
                anchors.horizontalCenter: parent.horizontalCenter; anchors.top: input.bottom; anchors.topMargin: 5
                color: input.activeFocus ? "#3fc1e6" : "white"
                Behavior on color { ColorAnimation { duration: 200 } }
                font.pointSize: 8
            }
        }

        ColumnLayout {
            id: popupLayout
            anchors.fill: parent; anchors.margins: 20
            spacing: 20

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: "拾色器"; font.pointSize: 12; color: "white";
                }
                Rectangle {
                    Layout.preferredWidth: 60; Layout.preferredHeight: 28; radius: 14
                    color: "transparent"
                    border.color: resetHover.hovered ? Qt.rgba(1, 1, 1, 0.2) : "transparent"
                    border.width: 1
                    Text {
                        anchors.centerIn: parent;
                        text: "恢复默认"
                        color: resetHover.hovered ? "white" : Qt.rgba(1, 1, 1, 0.6)
                        font.pointSize: 9
                    }
                    HoverHandler { id: resetHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler {
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onTapped: {
                            colorPopup.syncFromColor("#1E1E28")
                            colorPopup.updateBackgroundColor()
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true; height: 180
                spacing: 16

                // SV 面板
                Item {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    // 基础色相层
                    Rectangle { anchors.fill: parent; color: Qt.hsva(colorPopup.hue, 1, 1, 1) }
                    // 水平饱和度渐变层
                    Rectangle {
                        anchors.fill: parent
                        // 低饱和度(左) -> 高饱和度(右)
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#FFFFFFFF" } // 白色
                            GradientStop { position: 1.0; color: "#00FFFFFF" } // 透明白色
                        }
                    }
                    // 垂直明度渐变层
                    Rectangle {
                        anchors.fill: parent
                        // 高明度(上) -> 低明度(下)
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#00000000" } // 透明黑色
                            GradientStop { position: 1.0; color: "#FF000000" } // 黑色
                        }
                    }

                    MouseArea {
                        id: svMouseArea; anchors.fill: parent
                        function updateSV(mouse) {
                            colorPopup.sat = Math.max(0, Math.min(1, mouse.x / width));
                            colorPopup.val = 1 - Math.max(0, Math.min(1, mouse.y / height));
                            colorPopup.updateBackgroundColor();
                        }
                        cursorShape: Qt.PointingHandCursor
                        onPressed: function(mouse) { updateSV(mouse) }
                        onPositionChanged: function(mouse) { updateSV(mouse) }
                    }

                    Rectangle {
                        x: colorPopup.sat * parent.width - width / 2;
                        y: (1 - colorPopup.val) * parent.height - height / 2
                        width: 12; height: 12
                        scale: svMouseArea.pressed ? 1.3 : 1.0
                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                        radius: width / 2; color: "transparent"
                        border.color: "white"; border.width: 2
                    }
                }

                // Hue Slider
                Rectangle {
                    width: 16; Layout.fillHeight: true
                    radius: 8
                    border.color: Qt.rgba(1, 1, 1, 0.1); border.width: 1
                    gradient: Gradient {
                        GradientStop { position: 0.000; color: "#FF0000" }
                        GradientStop { position: 0.166; color: "#FF00FF" }
                        GradientStop { position: 0.333; color: "#0000FF" }
                        GradientStop { position: 0.500; color: "#00FFFF" }
                        GradientStop { position: 0.666; color: "#00FF00" }
                        GradientStop { position: 0.833; color: "#FFFF00" }
                        GradientStop { position: 1.000; color: "#FF0000" }
                    }

                    MouseArea {
                        id: hMouseArea; anchors.fill: parent
                        function updateH(mouse) {
                            colorPopup.hue = 1 - Math.max(0, Math.min(1, mouse.y / height))
                            colorPopup.updateBackgroundColor()
                        }
                        cursorShape: Qt.PointingHandCursor
                        onPressed: function(mouse) { updateH(mouse) }
                        onPositionChanged: function(mouse) { updateH(mouse) }
                    }
                    Rectangle {
                        y: (1 - colorPopup.hue) * parent.height - height / 2
                        Behavior on y {
                            NumberAnimation {
                                duration: 200
                                easing.type: Easing.OutSine
                            }
                        }
                        width: 16; height: 16
                        radius: 8; color: "white"
                        scale: hMouseArea.pressed ? 1.1 : 1.0
                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                    }
                }

                // Alpha Slider
                Item {
                    width: 16; Layout.fillHeight: true

                    Checkerboard {
                        anchors.fill: parent
                        radius: 8
                    }
                    Rectangle {
                        anchors.fill: parent; radius: 8
                        border.color: Qt.rgba(1, 1, 1, 0.1); border.width: 1
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Qt.hsva(colorPopup.hue, colorPopup.sat, colorPopup.val, 1.0) }
                            GradientStop { position: 1.0; color: Qt.hsva(colorPopup.hue, colorPopup.sat, colorPopup.val, 0.0) }
                        }
                    }
                    MouseArea {
                        id: aMouseArea; anchors.fill: parent
                        function updateA(mouse) {
                            colorPopup.alpha = 1 - Math.max(0, Math.min(1, mouse.y / height))
                            colorPopup.updateBackgroundColor()
                        }
                        cursorShape: Qt.PointingHandCursor
                        onPressed: function(mouse) { updateA(mouse) }
                        onPositionChanged: function(mouse) { updateA(mouse) }
                    }
                    Rectangle {
                        y: (1 - colorPopup.alpha) * parent.height - height / 2
                        Behavior on y {
                            NumberAnimation {
                                duration: 200
                                easing.type: Easing.OutSine
                            }
                        }
                        width: 16; height: 16
                        radius: 8; color: "white"
                        scale: aMouseArea.pressed ? 1.1 : 1.0
                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                    }
                }
            }

            ColumnLayout {
                id: inputContainer
                Layout.fillWidth: true
                spacing: 12
                property int inputMode: 0 // 0: HEX, 1: RGB, 2: HSL, 3: HSV

                Rectangle {
                    Layout.fillWidth: true; height: 32
                    radius: 8; color: Qt.rgba(0, 0, 0, 0.3)
                    border.color: Qt.rgba(1, 1, 1, 0.1); border.width: 1

                    // 选中项高亮滑块
                    Rectangle {
                        width: (parent.width - 6) / 4; height: parent.height - 6
                        y: 3; x: 3 + inputContainer.inputMode * width
                        Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutSine } }
                        radius: 6; color: Qt.rgba(1, 1, 1, 0.15)
                        border.color: Qt.rgba(1, 1, 1, 0.1); border.width: 1
                    }

                    // 颜色模型选项
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 3
                        Repeater {
                            model: ["HEX", "RGB", "HSL", "HSV"]
                            Item {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: inputContainer.inputMode === index ? "white" : Qt.rgba(1, 1, 1, 0.6)
                                    Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
                                    font.pointSize: 9
                                }
                                MouseArea { anchors.fill: parent; onClicked: inputContainer.inputMode = index }
                            }
                        }
                    }
                }

                // 颜色输入
                RowLayout {
                    Layout.fillWidth: true; spacing: 10

                    // HEX
                    ChannelInput {
                        visible: inputContainer.inputMode === 0
                        label: "HEX"
                        isHex: true
                        valueText: colorPopup.currColor.a < 1.0 ? colorPopup.currColor.toString().toUpperCase()
                                                                : colorPopup.currColor.toString().toUpperCase().substring(0, 7)
                        onValueEdited: function(val) {
                            let hex = val.trim();
                            if(hex.startsWith("#")) hex = hex.substring(1);
                            if([3, 4, 6, 8].includes(hex.length)) {
                                let parsed = Qt.color("#" + hex)
                                if(parsed.toString() !== "") {
                                    colorPopup.syncFromColor(parsed)
                                    colorPopup.updateBackgroundColor()
                                }
                            }
                        }
                    }

                    // Ch 1 (R/H)
                    ChannelInput {
                        visible: inputContainer.inputMode !== 0
                        label: inputContainer.inputMode === 1 ? "R" : "H"
                        valueText: inputContainer.inputMode === 1 ? Math.round(colorPopup.currColor.r * 255)
                                                                  : Math.round(colorPopup.hue * 360)
                        onValueEdited: function(val) {
                            let v = parseInt(val)
                            if(isNaN(v)) return
                            if(inputContainer.inputMode === 1) { // RGB
                                v = Math.max(0, Math.min(255, v))
                                colorPopup.syncFromColor(Qt.rgba(v/255, colorPopup.currColor.g, colorPopup.currColor.b, colorPopup.currColor.a));
                            } else if(inputContainer.inputMode === 2) { // HSL
                                v = Math.max(0, Math.min(360, v))
                                colorPopup.setFromHsl(v/360, colorPopup.getHsl().s, colorPopup.getHsl().l);
                            } else if(inputContainer.inputMode === 3) { // HSV
                                v = Math.max(0, Math.min(360, v))
                                colorPopup.hue = v/360;
                            }
                            colorPopup.updateBackgroundColor();
                        }
                    }

                    // Ch 2 (G/S)
                    ChannelInput {
                        visible: inputContainer.inputMode !== 0
                        label: inputContainer.inputMode === 1 ? "G" : "S%"
                        valueText: inputContainer.inputMode === 1 ? Math.round(colorPopup.currColor.g * 255)
                                                                  : (inputContainer.inputMode === 2 ? Math.round(colorPopup.getHsl().s * 100)
                                                                                                    : Math.round(colorPopup.sat * 100))
                        onValueEdited: function(val) {
                            let v = parseInt(val)
                            if(isNaN(v)) return
                            if(inputContainer.inputMode === 1) { // RGB
                                v = Math.max(0, Math.min(255, v))
                                colorPopup.syncFromColor(Qt.rgba(colorPopup.currColor.r, v/255, colorPopup.currColor.b, colorPopup.currColor.a));
                            } else if(inputContainer.inputMode === 2) { // HSL
                                v = Math.max(0, Math.min(100, v))
                                colorPopup.setFromHsl(colorPopup.getHsl().h, v/100, colorPopup.getHsl().l);
                            } else if(inputContainer.inputMode === 3) { // HSV
                                v = Math.max(0, Math.min(100, v))
                                colorPopup.sat = v/100;
                            }
                            colorPopup.updateBackgroundColor();
                        }
                    }

                    // Ch 3 (B/L/V)
                    ChannelInput {
                        visible: inputContainer.inputMode !== 0
                        label: inputContainer.inputMode === 1 ? "B" : (inputContainer.inputMode === 2 ? "L%" : "V%")
                        valueText: inputContainer.inputMode === 1 ? Math.round(colorPopup.currColor.b * 255)
                                                                  : (inputContainer.inputMode === 2 ? Math.round(colorPopup.getHsl().l * 100)
                                                                                                    : Math.round(colorPopup.val * 100))
                        onValueEdited: function(val) {
                            let v = parseInt(val)
                            if(isNaN(v)) return
                            if(inputContainer.inputMode === 1) { // RGB
                                v = Math.max(0, Math.min(255, v))
                                colorPopup.syncFromColor(Qt.rgba(colorPopup.currColor.r, colorPopup.currColor.g, v/255, colorPopup.currColor.a));
                            } else if(inputContainer.inputMode === 2) { // HSL
                                v = Math.max(0, Math.min(100, v))
                                colorPopup.setFromHsl(colorPopup.getHsl().h, colorPopup.getHsl().s, v/100);
                            } else if(inputContainer.inputMode === 3) { // HSV
                                v = Math.max(0, Math.min(100, v))
                                colorPopup.val = v/100;
                            }
                            colorPopup.updateBackgroundColor();
                        }
                    }

                    // Ch 4 (Alpha)
                    ChannelInput {
                        visible: inputContainer.inputMode !== 0
                        label: "A%"
                        valueText: Math.round(colorPopup.alpha * 100)
                        onValueEdited: function(val) {
                            let v = parseInt(val)
                            if(isNaN(v)) return
                            v = Math.max(0, Math.min(100, v))
                            colorPopup.alpha = v / 100.0
                            colorPopup.updateBackgroundColor()
                        }
                    }
                }
            }
        }
    }


    // 背景图片选择器
    FileDialog {
        id: customBgFileDialog
        title: "选择图片"
        nameFilters: ["图片文件 (*.png *.jpg *.jpeg *.bmp)", "所有文件 (*)"]
        onAccepted: XCPlayer.SetCustomImageBackground(currentFile)
    }

    // 设置卡片
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

    // 功能开关行
    component FeatureSwitchRow: Rectangle {
        property string featureText: ""
        property string descText: ""
        property alias checked: featureSwitch.checked

        Layout.fillWidth: true
        implicitHeight: rowLayout.implicitHeight + 24
        color: rowHoverHandler.hovered ? Qt.rgba(1, 1, 1, 0.03) : "transparent"
        Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
        radius: 10

        HoverHandler { id: rowHoverHandler }
        TapHandler { onTapped: featureSwitch.checked = !featureSwitch.checked }

        RowLayout {
            id: rowLayout
            anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true; spacing: 5
                Text { text: featureText; color: "white"; font.pointSize: 11}
                Text {
                    Layout.fillWidth: true; wrapMode: Text.Wrap
                    text: descText; font.pointSize: 9; color: Qt.rgba(1, 1, 1, 0.6)
                }
            }

            Switch {
                id: featureSwitch
                Layout.alignment: Qt.AlignVCenter
                padding: 0
                background: null
                indicator: Rectangle {
                    implicitWidth: 44; implicitHeight: 24; radius: 12
                    color: featureSwitch.checked ? "#0A84FF" : Qt.rgba(255, 255, 255, 0.1)
                    Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
                    HoverHandler { id: switchHoverHandler; cursorShape: Qt.PointingHandCursor }
                    Rectangle {
                        x: featureSwitch.checked ? parent.width - width : 0; y: 2
                        Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                        width: 20; height: 20
                        radius: 10
                        color: "white"
                        scale: switchHoverHandler.hovered ? 1.1 : 1.0
                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                    }
                }
            }
        }
    }

    // 颜色选择器行
    component ColorSelectorRow: Rectangle {
        property string featureText: ""
        property string descText: ""

        Layout.fillWidth: true
        implicitHeight: colorRowLayout.implicitHeight + 24
        color: colorRowHoverHandler.hovered ? Qt.rgba(1, 1, 1, 0.03) : "transparent"
        Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
        radius: 10

        HoverHandler { id: colorRowHoverHandler }
        TapHandler { onTapped: colorPopup.open() }

        RowLayout {
            id: colorRowLayout
            anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 28
            spacing: 16
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5
                Text { text: featureText; color: "white"; font.pointSize: 11 }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.Wrap
                    text: descText; font.pointSize: 9; color: Qt.rgba(1, 1, 1, 0.6)
                }
            }

            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: 32; implicitHeight: 32; radius: 16
                color: window.backgroundColor
                border.color: Qt.rgba(1, 1, 1, 0.2); border.width: 1
                scale: colorHoverHandler.hovered ? 1.1 : 1.0
                Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                HoverHandler { id: colorHoverHandler; cursorShape: Qt.PointingHandCursor }
            }
        }
    }

    // 滑动条行组件
    component SliderRow: Rectangle {
        property string featureText: ""
        property string descText: ""
        property alias from: sld.from
        property alias to: sld.to
        property alias stepSize: sld.stepSize
        property alias value: sld.value

        Layout.fillWidth: true
        implicitHeight: sliderRowLayout.implicitHeight + 24
        color: sliderRowHover.hovered ? Qt.rgba(1, 1, 1, 0.03) : "transparent"
        Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
        radius: 10
        HoverHandler { id: sliderRowHover }

        RowLayout {
            id: sliderRowLayout
            anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true; spacing: 5
                Text { text: featureText; color: "white"; font.pointSize: 11}
                Text {
                    Layout.fillWidth: true; wrapMode: Text.Wrap
                    text: descText; font.pointSize: 9; color: Qt.rgba(1, 1, 1, 0.6)
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignVCenter
                spacing: 10
                Slider {
                    id: sld
                    focusPolicy: Qt.NoFocus
                    Layout.preferredWidth: 120

                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }
                    WheelHandler {
                        onWheel: (wheel) => {
                                     if(wheel.angleDelta.y > 0) sld.increase()
                                     else if(wheel.angleDelta.y < 0) sld.decrease()
                                 }
                    }

                    background: Rectangle {
                        x: sld.leftPadding
                        y: sld.topPadding + (sld.availableHeight - height) / 2
                        width: sld.availableWidth
                        height: 10
                        radius: 5
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
                            radius: 5
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
                        width: 18
                        height: 18
                        radius: 9
                        scale: sld.pressed ? 1.1 : 1.0
                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                        color: "white"
                        border.color: Qt.rgba(0, 0, 0, 0.15)
                        border.width: 1
                    }
                }

                Text {
                    text: sld.value.toFixed(1)
                    color: "white"
                    font.pointSize: 10
                    Layout.preferredWidth: 30
                    horizontalAlignment: Text.AlignRight
                }
            }
        }
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        padding: 24

        ScrollBar.vertical: ScrollBar {
            id: vScrollBar
            anchors.right: scrollView.right; anchors.top: scrollView.top; anchors.bottom: scrollView.bottom
            implicitWidth: 10; minimumSize : 0.03; bottomPadding: 80
            contentItem: Rectangle {
                radius: width / 2; color: Qt.rgba(1, 1, 1, 0.4)
                opacity: vScrollBar.active ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutSine } }
            }
        }

        ColumnLayout {
            id: contentLayout
            width: scrollView.availableWidth
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true; spacing: 10
                Text { text: "通用"; color: "white"; font.pointSize: 15; font.bold: true }
                SettingsCard {
                    FeatureSwitchRow {
                        featureText: "悬浮窗口"; descText: "关闭主窗口后，在屏幕保留极简的迷你播放器。(提示：鼠标置于该窗口上时可以通过滚轮修改音量)"
                        checked: window.floatingWindow; onCheckedChanged: window.floatingWindow = checked
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true; spacing: 10
                Text { text: "外观"; color: "white"; font.pointSize: 15; font.bold: true }
                SettingsCard {
                    ColorSelectorRow { featureText: "纯色背景"; descText: "选择喜欢的颜色作为背景。" }
                    Rectangle { Layout.fillWidth: true; height: 1; Layout.leftMargin: 12; Layout.rightMargin: 12; color: Qt.rgba(1, 1, 1, 0.08) }

                    FeatureSwitchRow {
                        featureText: "封面背景"; descText: "使用当前媒体封面作为背景。"
                        checked: window.coverBackground; onCheckedChanged: window.coverBackground = checked
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; Layout.leftMargin: 12; Layout.rightMargin: 12; color: Qt.rgba(1, 1, 1, 0.08) }

                    FeatureSwitchRow {
                        featureText: "自定义背景"; descText: "选择喜欢的图片作为背景。"
                        checked: window.customBackground; onCheckedChanged: window.customBackground = checked
                    }

                    Item {
                        Layout.fillWidth: true
                        implicitHeight: 30
                        visible: window.customBackground

                        Button {
                            anchors.right: parent.right
                            anchors.rightMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            implicitWidth: 80
                            implicitHeight: 28
                            background: Rectangle {
                                color: parent.hovered ? Qt.rgba(63/255, 193/255, 230/255, 0.4)
                                                      : Qt.rgba(63/255, 193/255, 230/255, 0.1)
                                Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutSine } }
                                border.color: "#2B95B3"
                                radius: 5
                            }
                            contentItem: Text {
                                text: "选择图片"; color: "white"
                                font.pointSize: 9
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: customBgFileDialog.open()
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; Layout.leftMargin: 12; Layout.rightMargin: 12; color: Qt.rgba(1, 1, 1, 0.08) }

                    SliderRow {
                        featureText: "背景图片透明度"
                        descText: "调整背景图片的透明度。"
                        from: 0; to: 1; stepSize: 0.1
                        value: window.imageOpacity
                        onValueChanged: window.imageOpacity = value
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; Layout.leftMargin: 12; Layout.rightMargin: 12; color: Qt.rgba(1, 1, 1, 0.08) }

                    SliderRow {
                        featureText: "背景图片模糊强度"
                        descText: "调整背景图片的模糊程度。"
                        from: 0; to: 100; stepSize: 1.0
                        value: window.blurRadius
                        onValueChanged: window.blurRadius = value
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; Layout.leftMargin: 12; Layout.rightMargin: 12; color: Qt.rgba(1, 1, 1, 0.08) }

                    SliderRow {
                        featureText: "封面模糊强度"
                        descText: "调整沉浸播放时的音乐封面模糊程度。"
                        from: 0; to: 100; stepSize: 1.0
                        value: window.displayBlurRadius
                        onValueChanged: window.displayBlurRadius  = value
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; Layout.leftMargin: 12; Layout.rightMargin: 12; color: Qt.rgba(1, 1, 1, 0.08) }

                    FeatureSwitchRow {
                        featureText: "浮光漾影"
                        descText: "浮光随日度，漾影逐波深。（请注意：开启此项会增加CPU和GPU功耗）"
                        checked: window.dynamicEffect; onCheckedChanged: window.dynamicEffect = checked
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true; spacing: 10
                Text { text: "音频"; color: "white"; font.pointSize: 15; font.bold: true }
                SettingsCard {
                    FeatureSwitchRow {
                        featureText: "WASAPI 独占模式"
                        descText: "绕过系统混音器，获得纯净音质体验。（请注意：开启后会阻止其他程序播放声音）"
                        checked: window.exclusiveMode; onCheckedChanged: window.exclusiveMode = checked
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; Layout.leftMargin: 12; Layout.rightMargin: 12; color: Qt.rgba(1, 1, 1, 0.08) }

                    FeatureSwitchRow {
                        featureText: "回放增益（Replay Gain）"
                        descText: "读取音频的感知响度信息，调整每首歌至标准音量。（仅对有 Replay Gain 数据的音频生效）"
                        checked: window.replayGain; onCheckedChanged: window.replayGain = checked
                    }
                }
            }

            Item { height: 80 }
        }
    }
}
