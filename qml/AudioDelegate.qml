import QtQuick
import QtQuick.Controls
import XCPlayer

Rectangle {
    id: root
    width: ListView.view.width
    height: 64
    radius: 5
    color: itemSelected  ? Qt.rgba(64/255, 128/255, 192/255, 0.16)
                         : hoverHandler.hovered ? Qt.rgba(1, 1, 1, 0.08)
                                                : "transparent"
    Behavior on color {
        ColorAnimation {
            duration: 100
            easing.type: Easing.OutSine
        }
    }

    property MyListView listView
    property Menu menu
    property int listID
    property bool itemSelected: listView.isSelected(index)

    property color titleColor: root.itemSelected ? "#3fc1e6" : "white"
    property color textColor: root.itemSelected  ? "#3fc1e6" : "lightgray"


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
                          if(listView.selectionMap[index] !== true) {
                              listView.handleSelection(index, Qt.NoModifier)
                          }
                          menu.popup()
                      }
                  }

        // 双击播放
        onDoubleTapped: (eventPoint, button) => {
                            if(button === Qt.LeftButton) {
                                listView.handleSelection(index, Qt.NoModifier)
                                XCPlayer.Play(model.id, listID)
                            }
                        }
    }

    // 选中时左侧的高光条
    Rectangle {
        visible: root.itemSelected
        width: 4
        height: parent.height
        radius: 3
        color: "#3fc1e6"
    }

    // 封面小图
    Image {
        id: coverImage
        width: 48
        height: 48
        anchors {
            left: parent.left
            leftMargin: 12
            verticalCenter: parent.verticalCenter
        }
        source: model.cover
        sourceSize: Qt.size(48, 48)
        fillMode: Image.PreserveAspectFit
        cache: false
    }

    // 标题
    Text {
        anchors {
            left: coverImage.right
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

    // 艺术家
    Text {
        id: artistText
        anchors {
            left: coverImage.right
            leftMargin: 20
            bottom: coverImage.bottom
        }
        width: parent.width / 2 - x
        text: model.artist
        color: root.textColor
        font.pointSize: 9
        elide: Text.ElideRight
    }

    // 专辑
    Text {
        anchors {
            left: artistText.right
            right: durationText.left
            leftMargin: 20
            rightMargin: 20
            bottom: coverImage.bottom
        }
        text: model.album
        color: root.textColor
        font.pointSize: 9
        elide: Text.ElideRight
    }

    // 时长
    Text {
        id: durationText
        anchors {
            right: parent.right
            rightMargin: 48
            bottom: coverImage.bottom
        }
        text: XCPlayer.FormatDuration(model.duration)
        color: root.textColor
        font.pointSize: 9
        horizontalAlignment: Text.AlignRight
    }
}
