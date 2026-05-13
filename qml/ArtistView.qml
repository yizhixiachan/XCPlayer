import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XCPlayer

StackView {
    id: root
    clip: true
    initialItem: artistPageComponent

    // 头像生成
    function getAvatarColor(name) {
        let colors = ["#E91E63", "#9C27B0", "#673AB7", "#3F51B5", "#03A9F4", "#009688", "#4CAF50", "#FF9800", "#795548", "#607D8B"]
        let hash = 0;
        for (let i = 0; i < name.length; i++) {
            hash = name.charCodeAt(i) + ((hash << 5) - hash);
        }
        return colors[Math.abs(hash) % colors.length];
    }

    // 更新信号
    signal albumPageUpdated()
    signal artistPageUpdated()

    // 艺术家列表页面
    Component {
        id: artistPageComponent

        Item {
            id: artistPage

            property var allArtistsInfo: [] // 存储所有艺术家原始数据
            property bool sortAscending: true // 排序状态

            Component.onCompleted: {
                allArtistsInfo = XCPlayer.LoadArtists()
                updateModel()
            }

            Connections {
                target: root
                function onArtistPageUpdated() {
                    allArtistsInfo = XCPlayer.LoadArtists()
                    updateModel()
                }
            }

            function updateModel() {
                let filterText = searchEdit.text.toLowerCase()
                let filtered = allArtistsInfo.filter(name => name.toLowerCase().includes(filterText))

                filtered.sort((a, b) => {
                                  let res = a.localeCompare(b)
                                  return sortAscending ? res : -res
                              })

                artistListModel.clear()
                for (let i = 0; i < filtered.length; i++) {
                    artistListModel.append({ "artistName": filtered[i] })
                }
            }

            ListModel { id: artistListModel }

            ColumnLayout {
                anchors.fill: parent

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 12
                    spacing: 5

                    // 标题
                    Text {
                        text: "艺术家 - " + artistListModel.count
                        color: "white"
                        font.pointSize: 14
                    }

                    Item { Layout.fillWidth: true }

                    // 排序按钮
                    MyButton {
                        id: sortBtn
                        radius: 5
                        icon.source: "qrc:/assets/icons/Sort.png"
                        icon.width: 20
                        icon.height: 20
                        ToolTip.text: "排序"

                        rotation: artistPage.sortAscending ? 0 : 180
                        Behavior on rotation {
                            NumberAnimation { duration: 200; easing.type: Easing.OutSine }
                        }

                        onClicked: {
                            artistPage.sortAscending = !artistPage.sortAscending
                            artistPage.updateModel()
                        }
                    }

                    // 搜索按钮
                    MyButton {
                        visible: !searchEdit.isOpen
                        radius: 5
                        icon.source: "qrc:/assets/icons/Search.png"
                        icon.width: 20; icon.height: 20
                        ToolTip.text: "搜索"
                        onClicked: {
                            searchEdit.isOpen = true
                            searchEdit.forceActiveFocus()
                        }
                    }

                    // 搜索输入框
                    TextField {
                        id: searchEdit
                        visible: opacity > 0
                        opacity: isOpen ? 1.0 : 0.0
                        Layout.preferredWidth: isOpen ? 150 : 0
                        Layout.minimumWidth: 0
                        Behavior on Layout.preferredWidth { NumberAnimation { duration: 200; easing.type: Easing.OutSine } }
                        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutSine } }

                        focus: isOpen
                        placeholderText: "搜索"
                        placeholderTextColor: Qt.rgba(1, 1, 1, 0.5)
                        selectedTextColor: "white"
                        selectionColor: "#0A84FF"
                        color: "white"
                        font.pointSize: 10
                        verticalAlignment: TextInput.AlignVCenter
                        leftPadding: 12; rightPadding: 12

                        property bool isOpen: false

                        background: Rectangle {
                            color: Qt.rgba(120/255, 120/255, 120/255, 0.1)
                            border.color: searchEdit.activeFocus ? "#3fc1e6" : "#333333"
                            border.width: 1
                            radius: 15
                        }

                        onTextChanged: artistPage.updateModel()
                    }

                    // 收起搜索框按钮
                    MyButton {
                        visible: searchEdit.isOpen
                        radius: 5
                        icon.source: "qrc:/assets/icons/Right.png"
                        icon.width: 20; icon.height: 20
                        ToolTip.text: "收起"
                        onClicked: {
                            searchEdit.isOpen = false
                            searchEdit.clear()
                        }
                    }
                }

                // 艺术家网格列表
                GridView {
                    id: artistGridView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: artistListModel

                    property int minCellWidth: 220
                    property int columns: Math.max(1, Math.floor(width / minCellWidth))
                    cellWidth: Math.floor(width / columns)
                    cellHeight: 64

                    footer: Item { height: 80 }
                    ScrollBar.vertical: ScrollBar {
                        id: vScrollBar
                        implicitWidth: 10
                        minimumSize : 0.03
                        bottomPadding: 80

                        contentItem: Rectangle {
                            radius: width / 2
                            color: Qt.rgba(1, 1, 1, 0.4)
                            opacity: vScrollBar.active ? 1.0 : 0.0
                            Behavior on opacity {
                                NumberAnimation { duration: 500; easing.type: Easing.OutSine }
                            }
                        }
                    }

                    delegate: Item {
                        width: artistGridView.cellWidth
                        height: artistGridView.cellHeight

                        Rectangle {
                            anchors.fill: parent
                            color: hoverHandler.hovered ? Qt.rgba(1,1,1,0.1) : "transparent"
                            radius: 5

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 12

                                // 头像圆圈
                                Rectangle {
                                    width: 48; height: 48
                                    radius: 24
                                    color: root.getAvatarColor(model.artistName)
                                    Text {
                                        anchors.centerIn: parent
                                        text: model.artistName.charAt(0).toUpperCase()
                                        color: "white"
                                        font.pointSize: 18
                                        font.bold: true
                                    }
                                }

                                // 艺术家名字
                                Text {
                                    Layout.fillWidth: true
                                    text: model.artistName
                                    color: "white"
                                    font.pointSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            HoverHandler {
                                id: hoverHandler
                            }
                            TapHandler {
                                onTapped: {
                                    albumModel.LoadAlbumsFromArtist(model.artistName)
                                    root.push(albumPageComponent, { "artistName": model.artistName })
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 专辑列表页面
    Component {
        id: albumPageComponent

        Item {
            id: albumPage
            property string artistName: ""

            Connections {
                target: root
                function onAlbumPageUpdated() {
                    albumModel.LoadAlbumsFromArtist(artistName)
                }
            }

            ColumnLayout {
                anchors.fill: parent

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 20

                    MyButton {
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        radius: 18
                        icon.source: "qrc:/assets/icons/Left.png"
                        icon.width: 20
                        icon.height: 20
                        ToolTip.text: "返回"
                        onClicked: root.pop()
                    }

                    Rectangle {
                        width: 64; height: 64
                        radius: 32
                        color: root.getAvatarColor(artistName)
                        Text {
                            anchors.centerIn: parent
                            text: artistName.charAt(0).toUpperCase()
                            color: "white"
                            font.pointSize: 28
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        Text {
                            text: artistName
                            color: "white"
                            font.pointSize: 20
                            elide: Text.ElideRight
                        }
                        Text {
                            text: albumGridView.count + " 张专辑"
                            color: "#3fc1e6"
                            font.pointSize: 10
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                // 专辑网格列表
                GridView {
                    id: albumGridView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: albumModel

                    property int minCellWidth: 180
                    property int columns: Math.max(1, Math.floor(width / minCellWidth))
                    cellWidth: Math.floor(width / columns)
                    cellHeight: cellWidth + 50

                    footer: Item { height: 80 }
                    ScrollBar.vertical: ScrollBar {
                        id: vScrollBar
                        implicitWidth: 10
                        minimumSize : 0.03
                        bottomPadding: 80

                        contentItem: Rectangle {
                            radius: width / 2
                            color: Qt.rgba(1, 1, 1, 0.4)
                            opacity: vScrollBar.active ? 1.0 : 0.0
                            Behavior on opacity {
                                NumberAnimation { duration: 500; easing.type: Easing.OutSine }
                            }
                        }
                    }

                    delegate: Item {
                        width: albumGridView.cellWidth
                        height: albumGridView.cellHeight

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 5
                            spacing: 3

                            // 专辑封面
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: width
                                color: "transparent"

                                Image {
                                    anchors.fill: parent
                                    source: model.cover
                                    fillMode: Image.PreserveAspectFit
                                    cache: false
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 8
                                    color: Qt.rgba(0, 0, 0, 0.4)
                                    opacity: itemHover.hovered ? 1.0 : 0.0
                                    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutSine } }
                                }

                                HoverHandler { id: itemHover }
                                TapHandler {
                                    onTapped: {
                                        mediaModel.LoadBaseInfoFromAlbum(artistName, model.name)
                                        root.push(albumSongsPageComponent, {
                                                      "artistName": artistName,
                                                      "albumName": model.name
                                                  })
                                    }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: model.name
                                color: "white"
                                font.bold: true
                                font.pointSize: 11
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: model.count + " 首歌曲"
                                color: "lightgray"
                                font.pointSize: 9
                            }

                            Item { Layout.fillHeight: true }
                        }
                    }
                }
            }
        }
    }

    // 专辑歌曲列表页面
    Component {
        id: albumSongsPageComponent

        Item {
            id: albumSongsPage
            property string artistName: ""
            property string albumName: ""
            onVisibleChanged: {
                if(visible) {
                    mediaModel.LoadBaseInfoFromAlbum(artistName, albumName)
                }
            }

            ColumnLayout {
                anchors.fill: parent

                MyButton {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: 18
                    icon.source: "qrc:/assets/icons/Left.png"
                    icon.width: 20
                    icon.height: 20
                    ToolTip.text: "返回"
                    onClicked: root.pop()
                }

                MediaView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    playlistModel: audioPlaylistModel
                    externMediaModel: mediaModel
                    externSortFilterModel: mediaProxyModel
                    listName: albumName
                    listID: 1
                    isVideo: false
                    enableDrop: false

                }
            }
        }
    }

}
