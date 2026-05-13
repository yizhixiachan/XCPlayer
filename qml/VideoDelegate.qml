import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XCPlayer

Rectangle {
    id: root
    width: ListView.view.width
    height: 80
    radius: 5
    color: itemSelected  ? Qt.rgba(64/255, 128/255, 192/255, 0.16)
                         : hoverHandler.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
    Behavior on color { ColorAnimation { duration: 100; easing.type: Easing.OutSine } }

    property MyListView listView
    property Menu menu
    property int listID
    property bool itemSelected: listView.isSelected(index)

    HoverHandler { id: hoverHandler }
    TapHandler {
        id: tapHandler
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onTapped: (eventPoint, button) => {
                      listView.forceActiveFocus()
                      listView.currentIndex = index

                      if(button === Qt.LeftButton) {
                          listView.handleSelection(index, point.modifiers)
                      } else if(button === Qt.RightButton) {
                          // 右键菜单
                          if (listView.selectionMap[index] !== true) {
                              listView.handleSelection(index, Qt.NoModifier)
                          }
                          menu.popup()
                      }
                  }

        // 双击播放
        onDoubleTapped: (eventPoint, button) => {
                            if (button === Qt.LeftButton) {
                                listView.handleSelection(index, Qt.NoModifier)
                                XCPlayer.Play(model.id, listID)
                            }
                        }
    }

    property color titleColor: root.itemSelected ? "#3fc1e6" : "white"
    property color textColor: root.itemSelected  ? "#3fc1e6" : "lightgray"

    // 选中时左侧的高光条
    Rectangle {
        visible: root.itemSelected
        width: 4
        height: parent.height
        radius: 3
        color: "#3fc1e6"
    }

    // 封面小图
    Item {
        id: coverItem
        width: 96
        height: 54
        anchors {
            left: parent.left
            leftMargin: 12
            verticalCenter: parent.verticalCenter
        }

        Image {
            anchors.fill: parent
            source: model.cover
            fillMode: Image.PreserveAspectFit
            cache: false
        }

        // 时长遮罩框
        Rectangle {
            anchors {
                right: parent.right
                bottom: parent.bottom
                margins: 3
            }
            width: durText.width + 8; height: durText.height + 4
            color: Qt.rgba(0,0,0,0.75)
            radius: 3
            Text {
                id: durText
                anchors.centerIn: parent
                text: XCPlayer.FormatDuration(model.duration)
                color: "white"
                font.pointSize: 7
                font.bold: true
            }
        }
    }

    // 标题
    Text {
        anchors {
            left: coverItem.right
            right: parent.right
            leftMargin: 20
            rightMargin: 20
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: -5
        }
        text: model.title
        color: root.titleColor
        font.pointSize: 11
        elide: Text.ElideRight
    }

    RowLayout {
        anchors {
            left: coverItem.right
            right: parent.right
            leftMargin: 20
            bottom: coverItem.bottom
        }

        // 视频信息标签
        Row {
            id: tagRow
            spacing: 5

            property string resolution: model.resolution
            property real fps: model.fps
            property string hdrFormat: model.hdrFormat

            Repeater {
                model: {
                    let tags = [];

                    // 分辨率
                    if(tagRow.resolution) {
                        tags.push({
                                      text: tagRow.resolution,
                                      color: root.textColor,
                                      bg: Qt.rgba(1, 1, 1, 0.1),
                                      border: "transparent"
                                  })
                    }

                    // 帧率
                    if(tagRow.fps) {
                        tags.push({
                                      text: Number(tagRow.fps).toFixed(3) + " fps",
                                      color: root.textColor,
                                      bg: Qt.rgba(1, 1, 1, 0.1),
                                      border: "transparent"
                                  })
                    }

                    // HDR 格式
                    if(tagRow.hdrFormat) {
                        if(tagRow.hdrFormat === "SDR") {
                            tags.push({
                                          text: tagRow.hdrFormat,
                                          color: root.textColor,
                                          bg: Qt.rgba(1, 1, 1, 0.1),
                                          border: "transparent"
                                      })
                        } else {
                            tags.push({
                                          text: tagRow.hdrFormat,
                                          color: "#FFC107",
                                          bg: Qt.rgba(255/255, 193/255, 7/255, 0.15),
                                          border: "#FFC107"
                                      })
                        }
                    }

                    return tags;
                }

                Rectangle {
                    width: tagText.implicitWidth + 12
                    height: 18
                    radius: 3
                    color: modelData.bg
                    border.color: modelData.border
                    border.width: modelData.border === "transparent" ? 0 : 1

                    Text {
                        id: tagText
                        anchors.centerIn: parent
                        text: modelData.text
                        color: modelData.color
                        font.pointSize: 8
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }

        // 播放进度记录
        Text {
            id: progressText
            visible: text !== ""
            Layout.rightMargin: 48
            font.pointSize: 9
            color: root.textColor
            text: {
                if(model.duration > 0 && model.lastPosition > 0) {
                    let percent = (model.lastPosition / model.duration) * 100;
                    return percent.toFixed(0) + "%";
                }
                return "";
            }
        }
    }

}
