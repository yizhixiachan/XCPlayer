import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XCPlayer

Item {
    id: root

    property var protocolInfo: analyzeUrl(urlInput.text)

    property var uaModel:[
        {
            label: "Chrome (Win)",
            value: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36"
        },
        {
            label: "Safari (macOS)",
            value: "Mozilla/5.0 (Macintosh; Apple Silicon Mac OS X 14_5_0) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.5 Safari/605.1.15"
        },
        {
            label: "Safari (iPhone)",
            value: "Mozilla/5.0 (iPhone; CPU iPhone OS 17_5 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.5 Mobile/15E148 Safari/605.1.15"
        },
        {
            label: "Chrome (Android)",
            value: "Mozilla/5.0 (Linux; Android 14; Pixel 8 Pro) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/135.0.0.0 Mobile Safari/537.36"
        },
        { label: "VLC", value: "VLC/3.0.20 LibVLC/3.0.20" },
        { label: "FFmpeg", value: "Lavf/61.1.100" }
    ]

    readonly property bool canPlay:
        urlInput.text.trim() !== "" &&
        protocolInfo.supported

    ListModel {
        id: historyModel
    }

    ColumnLayout {
        anchors.fill: parent

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 5

            Text {
                text: "网络流（Beta）"
                color: "white"
                font.pointSize: 14
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.leftMargin: parent.width / 5
            Layout.rightMargin: parent.width / 5
            Layout.preferredHeight: 40

            TextField {
                id: urlInput
                anchors.fill: parent
                placeholderText: "请输入 HTTP(S) / RTMP 链接 或本地文件路径"
                placeholderTextColor: Qt.rgba(1, 1, 1, 0.5)
                selectedTextColor: "white"
                selectionColor: "#0A84FF"
                color: "white"
                font.pointSize: 10
                verticalAlignment: TextInput.AlignVCenter
                leftPadding: 12; rightPadding: 40

                background: Rectangle {
                    color: Qt.rgba(120/255, 120/255, 120/255, 0.1)
                    border.color: urlInput.activeFocus ? "#3fc1e6" : "#333333"
                    border.width: 1
                    radius: 20
                }

                onAccepted: startPlayback()
            }

            MyButton {
                id: playButton
                anchors.right: parent.right
                anchors.rightMargin: 3
                anchors.verticalCenter: parent.verticalCenter
                implicitWidth: 34
                implicitHeight: 34
                icon.source: "qrc:/assets/icons/Play.png"
                icon.width: 16; icon.height: 16
                iconHorizontalCenterOffset: 1
                radius: width / 2
                ToolTip.visible: false
                onClicked: startPlayback()
            }

        }

        Rectangle {
            visible: protocolInfo.type === "HTTP"
            implicitHeight: configLayout.implicitHeight + 24
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.leftMargin: 30
            Layout.rightMargin: 30
            Layout.bottomMargin: 12
            radius: 10
            color: "transparent"
            border.color: Qt.rgba(1, 1, 1, 0.2)
            border.width: 1

            ColumnLayout {
                id: configLayout
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Text {
                    text: "HTTP 请求配置（选填）"
                    color: "white"
                    font.pointSize: 11
                    font.bold: true
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    Text {
                        text: "User-Agent :"
                        color: "white"
                        font.pointSize: 10
                    }

                    ComboBox {
                        id: userAgentCombo
                        editable: true
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        currentIndex: -1
                        model: root.uaModel
                        textRole: "label"
                        onActivated: (index) => {
                                         userAgentCombo.editText = uaModel[index].value
                                     }

                        background: null

                        contentItem: TextField  {
                            id: userAgentInput
                            text: userAgentCombo.editText
                            placeholderText: "User-Agent"
                            placeholderTextColor: Qt.rgba(1, 1, 1, 0.5)
                            selectedTextColor: "white"
                            selectionColor: "#0A84FF"
                            color: "white"
                            font.pointSize: 10
                            verticalAlignment: TextInput.AlignVCenter
                            leftPadding: 12; rightPadding: 12

                            background: Rectangle {
                                color: Qt.rgba(120/255, 120/255, 120/255, 0.1)
                                border.color: userAgentInput.activeFocus ? "#3fc1e6" : "#333333"
                                border.width: 1
                                radius: 10
                            }
                        }

                        delegate: ItemDelegate {
                            width: 200
                            height: 30
                            highlighted: userAgentCombo.highlightedIndex === index

                            contentItem: Text {
                                text: modelData ? modelData.label : ""
                                color: "white"
                                font.pointSize: 10
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }

                            background: Rectangle {
                                radius: 3
                                color: itemHover.hovered ? Qt.rgba(160/255, 255/255, 255/255, 80/255) : "transparent"
                                HoverHandler {
                                    id: itemHover
                                }
                            }
                        }

                        popup: Popup {
                            x: userAgentCombo.width - contentItem.implicitWidth - 20
                            y: userAgentCombo.height
                            implicitWidth: contentItem.implicitWidth
                            implicitHeight: contentItem.implicitHeight
                            padding: 0

                            contentItem: ListView {
                                clip: true
                                implicitWidth: 200
                                implicitHeight: contentHeight
                                model: userAgentCombo.delegateModel
                                currentIndex: userAgentCombo.highlightedIndex
                            }

                            background: Rectangle {
                                color: Qt.rgba(40/255, 50/255, 70/255, 1)
                                border.color: Qt.rgba(70/255, 80/255, 100/255, 1)
                                border.width: 1
                                radius: 5
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    Text {
                        text: "Headers :"
                        color: "white"
                        font.pointSize: 10
                    }

                    TextArea {
                        id: headersInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 140
                        wrapMode: TextEdit.WrapAnywhere
                        placeholderText:
                            "例如：\nReferer: https://example.com\nCookie: session=xxx\nAuthorization: Bearer xxx"
                        placeholderTextColor: Qt.rgba(1, 1, 1, 0.5)
                        selectedTextColor: "white"
                        selectionColor: "#0A84FF"
                        color: "white"
                        font.pointSize: 10
                        leftPadding: 12; rightPadding: 12

                        background: Rectangle {
                            color: Qt.rgba(120/255, 120/255, 120/255, 0.1)
                            border.color: headersInput.activeFocus ? "#3fc1e6" : "#333333"
                            border.width: 1
                            radius: 10
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }

    function startPlayback() {
        let url = urlInput.text.trim()
        if(url === "" || !protocolInfo.supported) return

        let options = {}

        if(protocolInfo.type === "HTTP") {
            let ua = userAgentCombo.editText.trim()
            if(ua !== "")
                options["user_agent"] = ua

            let headers = headersInput.text.trim()
            if(headers !== "")
                options["headers"] = headers
        }

        XCPlayer.PlayTempUrl(url, options)
    }

    function schemeOf(urlText) {
        const u = (urlText || "").trim()
        // 匹配://
        const match = u.match(/^([a-zA-Z][a-zA-Z0-9+-.]*):\/\//)
        return match ? match[1].toLowerCase() : ""
    }

    function analyzeUrl(urlText) {

        const raw = (urlText || "").trim()

        let result = {
            supported: false,
            type: "NONE",
        };

        if(raw === "") return result

        const scheme = schemeOf(raw);

        // 判断是否为本地路径
        const isAbsolutePath = /^\/|^[a-zA-Z]:\\/.test(raw);

        if(scheme === "http" || scheme === "https") {
            result.supported = true
            result.type = "HTTP"
        }
        else if(scheme.startsWith("rtmp")) {
            result.supported = true
            result.type = "RTMP"
        }
        else if(isAbsolutePath) {
            result.supported = true
            result.type = "FILE"
        }

        return result
    }
}
