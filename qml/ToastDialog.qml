import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Popup {
    id: root
    parent: Overlay.overlay
    z: 999
    x: (parent.width - width) / 2
    y: parent.height / 9

    width: contentRow.implicitWidth + 40
    height: 42
    padding: 0
    margins: 0
    closePolicy: Popup.NoAutoClose

    background: Rectangle {
        radius: root.height / 2
        color: Qt.rgba(25/255, 30/255, 40/255, 0.8)
        border.color: Qt.rgba(1, 1, 1, 0.1)
        border.width: 1
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 300; easing.type: Easing.OutSine }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 300; easing.type: Easing.OutSine }
    }

    property bool isOpen: false
    property string messageText: ""
    property int messageType: 0 // 0: Busy, 1: Success, 2: Error, 3: Normal

    function show(type, text, duration) {
        root.messageType = type;
        root.messageText = text;
        root.isOpen = true;
        root.open();
        hideTimer.stop();
        if(duration > 0) {
            hideTimer.interval = duration;
            hideTimer.start();
        }
    }

    function hide() {
        root.isOpen = false;
        root.close();
    }

    Timer {
        id: hideTimer
        repeat: false
        onTriggered: root.hide()
    }

    contentItem: Item {
        RowLayout {
            id: contentRow
            anchors.centerIn: parent
            spacing: 12

            // 状态指示
            Item {
                width: 18; height: 18
                Layout.alignment: Qt.AlignVCenter
                visible: root.messageType !== 3

                // Processing 圆环
                Canvas {
                    anchors.fill: parent
                    visible: root.messageType === 0
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);
                        ctx.strokeStyle = "#3fc1e6";
                        ctx.lineWidth = 2.5;
                        ctx.lineCap = "round";
                        ctx.beginPath();
                        ctx.arc(width/2, height/2, width/2 - 2, 0, 1.5 * Math.PI);
                        ctx.stroke();
                    }
                    RotationAnimator on rotation {
                        from: 0; to: 360; duration: 800; loops: Animation.Infinite; running: parent.visible
                    }
                }

                // Success
                Text {
                    anchors.centerIn: parent
                    visible: root.messageType === 1
                    text: "✔"
                    color: "#10B981"
                    font.pointSize: 12
                }

                // Error
                Text {
                    anchors.centerIn: parent
                    visible: root.messageType === 2
                    text: "✖"
                    color: "#EF4444"
                    font.pointSize: 12
                }
            }

            // 信息
            Text {
                text: root.messageText
                color: "white"
                font.pointSize: 10
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }
}
