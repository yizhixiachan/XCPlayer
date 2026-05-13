import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import XCPlayer

Item {
    id: root

    property PlaylistModel playlistModel: null
    property MediaModel externMediaModel: null
    property SortFilterModel externSortFilterModel: null
    property string listName
    property int listID
    property bool isVideo

    property bool enableDrop: true

    signal sortChanged(int listID, int role, int order)
    signal mediaInfoShown(int mediaID)

    onListIDChanged: {
        searchEdit.clear()
        mediaList.clearSelection()
        Qt.callLater(() => mediaList.positionViewAtIndex(0, ListView.Beginning))
    }

    function clear() {
        searchEdit.clear()
        mediaList.clearSelection()
    }

    function locateItem(id) {
        searchEdit.clear()
        let idx = externSortFilterModel.GetIndexByID(id)
        mediaList.currentIndex = idx
        mediaList.positionViewAtIndex(idx, ListView.Center)
        mediaList.select(idx)
    }

    DropArea {
        id: dropArea
        anchors.fill: parent
        onEntered: (drag) => {
                       if(!root.enableDrop) {
                           drag.accepted = false
                       } else {
                           hasUrls = drag.hasUrls
                       }
                   }
        onExited: hasUrls = false
        onDropped: (drop) => {
                       hasUrls = false
                       if(drop.hasUrls) {
                           XCPlayer.ProcessUrls(drop.urls, listID)
                       } else {
                           drop.accepted = false
                       }
                   }
        property bool hasUrls: false

        // Drag 背景
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(120/255, 200/255, 240/255, 30/255)
            radius: 5
            visible: dropArea.containsDrag && dropArea.hasUrls
        }

        ColumnLayout {
            anchors.fill: parent

            // 顶部
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 12
                spacing: 5

                // 标题
                Text {
                    text: root.listName + " - " + mediaList.count
                    color: "white"
                    font.pointSize: 14
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
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
                    onClicked: sortMenu.popup(sortBtn, 0, sortBtn.height)

                    // 排序菜单
                    Menu {
                        id: sortMenu
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

                        ActionGroup { id: sortFieldGroup }

                        MyMenuItem {
                            text: "标题"
                            action: Action {
                                checkable: true
                                checked: true
                                ActionGroup.group: sortFieldGroup
                                property int data: MediaModel.TitleRole
                                onTriggered: {
                                    mediaList.clearSelection()
                                    externSortFilterModel.sortRole = data
                                    externSortFilterModel.Sort(sortDirectionGroup.checkedAction.data)
                                    root.sortChanged(root.listID, data, sortDirectionGroup.checkedAction.data)
                                }
                            }
                        }

                        MyMenuItem {
                            text: "艺术家"
                            visible: !root.isVideo
                            height: visible ? implicitHeight : 0
                            action: Action {
                                checkable: true
                                ActionGroup.group: sortFieldGroup
                                property int data: MediaModel.ArtistRole
                                onTriggered: {
                                    mediaList.clearSelection()
                                    externSortFilterModel.sortRole = data
                                    externSortFilterModel.Sort(sortDirectionGroup.checkedAction.data)
                                    root.sortChanged(root.listID, data, sortDirectionGroup.checkedAction.data)
                                }
                            }
                        }

                        MyMenuItem {
                            text: "专辑"
                            visible: !root.isVideo
                            height: visible ? implicitHeight : 0
                            action: Action {
                                checkable: true
                                ActionGroup.group: sortFieldGroup
                                property int data: MediaModel.AlbumRole
                                onTriggered: {
                                    mediaList.clearSelection()
                                    externSortFilterModel.sortRole = data
                                    externSortFilterModel.Sort(sortDirectionGroup.checkedAction.data)
                                    root.sortChanged(root.listID, data, sortDirectionGroup.checkedAction.data)
                                }
                            }
                        }

                        MyMenuItem {
                            text: "时长"
                            action: Action {
                                checkable: true
                                ActionGroup.group: sortFieldGroup
                                property int data: MediaModel.DurationRole
                                onTriggered: {
                                    mediaList.clearSelection()
                                    externSortFilterModel.sortRole = data
                                    externSortFilterModel.Sort(sortDirectionGroup.checkedAction.data)
                                    root.sortChanged(root.listID, data, sortDirectionGroup.checkedAction.data)
                                }
                            }
                        }

                        MenuSeparator {
                            topPadding: 3
                            bottomPadding: 3
                            contentItem: Rectangle {
                                implicitHeight: 1
                                color: Qt.rgba(70/255, 80/255, 100/255, 1) }
                        }

                        ActionGroup { id: sortDirectionGroup }

                        MyMenuItem {
                            text: "升序"
                            action: Action {
                                checkable: true
                                checked: true
                                ActionGroup.group: sortDirectionGroup
                                property int data: Qt.AscendingOrder
                                onTriggered: {
                                    mediaList.clearSelection()
                                    externSortFilterModel.Sort(data)
                                    root.sortChanged(root.listID, sortFieldGroup.checkedAction.data, data)
                                }
                            }
                        }
                        MyMenuItem {
                            text: "降序"
                            action: Action {
                                checkable: true
                                ActionGroup.group: sortDirectionGroup
                                property int data: Qt.DescendingOrder
                                onTriggered: {
                                    mediaList.clearSelection()
                                    externSortFilterModel.Sort(data)
                                    root.sortChanged(root.listID, sortFieldGroup.checkedAction.data, data)
                                }
                            }
                        }
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
                    Behavior on Layout.preferredWidth {
                        NumberAnimation {
                            duration: 200
                            easing.type: Easing.OutSine
                        }
                    }
                    Behavior on opacity {
                        NumberAnimation {
                            duration: 200
                            easing.type: Easing.OutSine
                        }
                    }

                    focus: isOpen
                    placeholderText: "搜索"
                    placeholderTextColor: Qt.rgba(1, 1, 1, 0.5)
                    selectedTextColor: "white"
                    selectionColor: "#0A84FF"

                    color: "white"
                    font.pointSize: 10
                    verticalAlignment: TextInput.AlignVCenter
                    leftPadding: 12
                    rightPadding: 12

                    property bool isOpen: false
                    property var savedPosition: ({ using: false, index: 0, offset: 0 })

                    background: Rectangle {
                        color: Qt.rgba(120/255, 120/255, 120/255, 0.1)
                        border.color: searchEdit.activeFocus ? "#3fc1e6" : "#333333"
                        border.width: 1
                        radius: 15
                    }

                    onTextChanged: {
                        mediaList.clearSelection()

                        // 保存搜索前位置
                        if(text.length > 0 && !savedPosition.using && mediaList.count > 0) {
                            savedPosition.using = true
                            savedPosition.index = mediaList.indexAt(0, mediaList.contentY)

                            let item = mediaList.itemAtIndex(savedPosition.index)
                            savedPosition.offset = item ? (mediaList.contentY - item.y) : 0
                        }

                        externSortFilterModel.setFilterFixedString(text)

                        if(text.length > 0) {
                            mediaList.positionViewAtIndex(0, ListView.Beginning)

                        } else if(savedPosition.using) {
                            // 恢复搜索前位置
                            mediaList.positionViewAtIndex(savedPosition.index, ListView.Beginning)
                            mediaList.contentY += savedPosition.offset
                            savedPosition.using = false
                        }
                    }
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

            // 空列表提示
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: mediaList.count === 0
                Column {
                    anchors.centerIn: parent
                    spacing: 10
                    visible: root.enableDrop

                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        source: "qrc:/assets/icons/Drop.png"
                        sourceSize: Qt.size(20, 20)
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "拖入文件或文件夹将自动导入"
                        color: "lightgray"
                        font.pointSize: 9
                    }
                }
            }

            // 媒体列表视图
            MyListView {
                id: mediaList
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: mediaList.count > 0
                model: externSortFilterModel
                delegate: root.isVideo ? videoDelegate : audioDelegate
                footer: Item { height: 80 }
                scrollBarBottomPadding: 80

                // 获取所有选中行的 mediaId
                function getSelectedIDArr() {
                    let selectedRows = Object.keys(selectionMap);3
                    let idArr = [];
                    for(let i = 0; i < selectedRows.length; i++) {
                        let row = parseInt(selectedRows[i]);
                        let id = externSortFilterModel.GetIDByIndex(row);
                        if(id !== -1) {
                            idArr.push(id);
                        }
                    }
                    return idArr;
                }

                Component {
                    id: audioDelegate
                    AudioDelegate {
                        listView: mediaList
                        menu: contextMenu
                        listID: root.listID
                    }
                }

                Component {
                    id: videoDelegate
                    VideoDelegate {
                        listView: mediaList
                        menu: contextMenu
                        listID: root.listID
                    }
                }
            }

        }

    }

    ConfirmDialog {
        id: confirmDialog
    }

    MetadataPopup {
        id: metadataPopup
    }


    // 菜单
    Menu {
        id: contextMenu
        delegate: MyMenuItem { leftPadding: 10 }
        implicitWidth: 180
        background: Rectangle {
            color: Qt.rgba(40/255, 50/255, 70/255, 1)
            border.color: Qt.rgba(70/255, 80/255, 100/255, 1)
            border.width: 1
            radius: 5
        }

        Menu {
            title: root.isVideo ? "添加到播单（" + mediaList.selectedCount + "）"
                                : "添加到歌单（" + mediaList.selectedCount + "）"
            implicitWidth: 150
            background: Rectangle {
                color: Qt.rgba(40/255, 50/255, 70/255, 1)
                border.color: Qt.rgba(70/255, 80/255, 100/255, 1)
                border.width: 1
                radius: 5
            }
            Repeater {
                model: root.playlistModel
                delegate: MyMenuItem {
                    text: model.name
                    leftPadding: 10
                    property bool isValid: model.listID > 2 && model.listID !== root.listID
                    visible: isValid
                    height: visible ? implicitHeight : 0
                    onTriggered: {
                        let count = mediaList.selectedCount;
                        if(count === 0) return;

                        let mediaType = root.isVideo ? "播单" : "歌单";
                        let targetListId = model.listID;

                        confirmDialog.message = `是否将选中的 ${count} 项添加到${mediaType}「${model.name}」？`;
                        confirmDialog.acceptCallback = function() {
                            let realIds = mediaList.getSelectedIDArr();
                            XCPlayer.AddBaseInfoToPlaylist(realIds, targetListId);
                        };
                        confirmDialog.open();
                    }
                }
            }
        }

        MyMenuItem {
            visible: !root.isVideo
            height: !root.isVideo ? implicitHeight : 0
            text: "修改元数据"
            leftPadding: 10
            enabled: mediaList.selectedCount === 1
            opacity: enabled ? 1.0 : 0.3
            onTriggered: {
                let idArr = mediaList.getSelectedIDArr();
                if(idArr.length === 1) {
                    metadataPopup.openEditor(idArr[0]);
                }
            }
        }

        MyMenuItem {
            text: "查看详细信息"
            leftPadding: 10
            enabled: mediaList.selectedCount === 1
            opacity: enabled ? 1.0 : 0.3
            onTriggered: {
                let idArr = mediaList.getSelectedIDArr();
                if(idArr.length === 1) {
                    root.mediaInfoShown(idArr[0])
                }
            }
        }

        MyMenuItem {
            text: "打开文件所在位置"
            leftPadding: 10
            enabled: mediaList.selectedCount === 1
            opacity: enabled ? 1.0 : 0.3
            onTriggered: {
                let idArr = mediaList.getSelectedIDArr();
                if(idArr.length === 1) {
                    XCPlayer.ShowInExplorer(idArr[0]);
                }
            }
        }

        MenuSeparator {
            topPadding: 3
            bottomPadding: 3
            contentItem: Rectangle {
                implicitHeight: 1
                color: Qt.rgba(70/255, 80/255, 100/255, 1)
            }
        }

        MyMenuItem {
            visible: true
            text: "设置为自定义背景"
            leftPadding: 10
            enabled: mediaList.selectedCount === 1
            opacity: enabled ? 1.0 : 0.3
            onTriggered: {
                let idArr = mediaList.getSelectedIDArr();
                if(idArr.length === 1) {
                    XCPlayer.SetCustomImageBackground("image://covers/large/" + idArr[0]);
                }
            }
        }

        MenuSeparator {
            topPadding: 3
            bottomPadding: 3
            contentItem: Rectangle {
                implicitHeight: 1
                color: Qt.rgba(70/255, 80/255, 100/255, 1)
            }
        }

        MyMenuItem {
            text: "删除选中项（" + mediaList.selectedCount + "）"
            leftPadding: 10
            onTriggered: {
                let count = mediaList.selectedCount;
                if (count === 0) return;

                confirmDialog.message = `是否从「${root.listName}」中删除选中的 ${count} 项？`;
                confirmDialog.acceptCallback = function() {
                    let idArr = mediaList.getSelectedIDArr();
                    externMediaModel.DeleteBaseInfo(idArr, root.listID);
                    mediaList.clearSelection();
                };
                confirmDialog.open();
            }
        }
    }


}
