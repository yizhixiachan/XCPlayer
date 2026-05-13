import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import XCPlayer

ColumnLayout {
    id: root
    spacing: 0

    property string title: ""
    property string iconSource: ""

    property PlaylistModel playlistModel: null

    property bool isVideo: false
    property bool isExpanded: false
    property int availableHeight: 0

    property int lastSelectedListID: isVideo ? 2 : 1
    property string lastSelectedListName: isVideo ? "全部视频" : "全部音乐"

    signal listSelected(int listID, string listName)
    signal createListRequested(string name)

    property var indexToIdMap: ({})

    function clearSelection() {
        playlistView.clearSelection()
    }

    function getSelectedListIds() {
        let ids = [];
        for (let idx in playlistView.selectionMap) {
            if (playlistView.selectionMap[idx] === true) {
                let listId = root.indexToIdMap[idx];
                if (listId !== undefined && listId !== 1 && listId !== 2) {
                    ids.push(listId);
                }
            }
        }
        return ids;
    }

    function selectPlaylistById(listID) {
        if(!root.playlistModel) return
        if(!root.isExpanded) root.isExpanded = true

        playlistView.clearSelection()
        root.lastSelectedListID = listID

        for(let i = 0; i < playlistView.count; i++) {
            let item = playlistView.itemAtIndex(i)
            if(item && item.listID === listID) {
                item.checked = true
                break
            }
        }

        root.listSelected(root.lastSelectedListID, root.lastSelectedListName)
    }

    state: isExpanded ? "expanded" : "collapsed"

    states: [
        State {
            name: "expanded"
            PropertyChanges { target: playlistView; Layout.preferredHeight: Math.min(playlistView.contentHeight, root.availableHeight) }
        },
        State {
            name: "collapsed"
            PropertyChanges { target: playlistView; Layout.preferredHeight: 0 }
        }
    ]

    transitions: Transition {
        NumberAnimation { property: "Layout.preferredHeight"; duration: 300; easing.type: Easing.OutQuad }
    }

    // 主按钮
    NavButton {
        Layout.fillWidth: true
        text: root.title
        icon.source: root.iconSource
        icon.width: 18; icon.height: 18
        checkable: false
        onClicked: {
            if(!root.isExpanded) {
                // 展开时，默认恢复记录的该歌单
                root.selectPlaylistById(root.lastSelectedListID);
            } else {
                root.isExpanded = false;
            }
        }
    }

    Item { height: 1}

    // 歌单按钮视图
    MyListView {
        id: playlistView
        Layout.fillWidth: true
        model: root.playlistModel
        spacing: 1

        isSelectable: function(idx) {
            let id = root.indexToIdMap[idx];
            return id !== 1 && id !== 2;
        }

        delegate: NavButton {
            id: navBtn
            anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            text: model.name
            listID: model.listID
            isChild: true
            checkable: true
            ButtonGroup.group: root.ButtonGroup.group

            isMultiSelected: playlistView.isSelected(index)

            Component.onCompleted: {
                root.indexToIdMap[index] = model.listID
                if(model.listID === root.lastSelectedListID) {
                    checked = true
                }
            }

            onNormalClicked: (id) => {
                                 playlistView.forceActiveFocus()

                                 if(id === 1 || id === 2) {
                                     playlistView.clearSelection()
                                 } else {
                                     playlistView.handleSelection(index, Qt.NoModifier)
                                 }
                                 root.lastSelectedListID = id
                                 root.lastSelectedListName = model.name
                                 root.listSelected(id, model.name)
                             }

            onMultiSelectRequested: (modifiers) => {
                                        playlistView.forceActiveFocus()
                                        playlistView.handleSelection(index, modifiers)
                                    }

            onRightClicked: (id, btnRef, pos) => {
                                if(!playlistView.isSelected(index)) {
                                    playlistView.handleSelection(index, Qt.NoModifier)
                                }
                                contextMenu.targetId = id
                                contextMenu.targetName = model.name
                                contextMenu.targetItem = btnRef
                                contextMenu.popup(btnRef, pos.x, pos.y)
                            }
        }

        footer: NavButton {
            id: addPlaylistBtn
            anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            icon.source: "qrc:/assets/icons/Add.png"
            icon.width: 16; icon.height: 16
            isChild: true
            checkable: false
            onClicked: {
                textFieldPopup.isRename = false
                textFieldPopup.targetItem = addPlaylistBtn
                textFieldPopup.open()
            }
        }
    }
    ConfirmDialog {
        id: confirmDialog
    }
    Menu {
        id: contextMenu
        property int targetId: -1
        property string targetName: ""
        property Item targetItem: null

        implicitWidth: 120
        delegate: MyMenuItem { leftPadding: 10 }
        background: Rectangle {
            color: Qt.rgba(40/255, 50/255, 70/255, 1)
            border.color: Qt.rgba(70/255, 80/255, 100/255, 1)
            border.width: 1
            radius: 5
        }

        MyMenuItem {
            text: "重命名"
            leftPadding: 10
            enabled: playlistView.selectedCount === 1
            opacity: enabled ? 1.0 : 0.3
            onTriggered: {
                textFieldPopup.isRename = true
                textFieldPopup.targetId = contextMenu.targetId
                textFieldPopup.targetName = contextMenu.targetName
                textFieldPopup.targetItem = contextMenu.targetItem
                textFieldPopup.open()
            }
        }

        MyMenuItem {
            text: "删除（" + playlistView.selectedCount + "）"
            leftPadding: 10
            onTriggered: {
                let count = playlistView.selectedCount

                confirmDialog.message = `是否删除选中的 ${count} 项？`
                confirmDialog.acceptCallback = function() {
                    let selectedIds = root.getSelectedListIds()
                    XCPlayer.DeletePlaylists(selectedIds)

                    // 如果删除的歌单包含正在选中的歌单，则回退到 默认歌单
                    if(selectedIds.includes(root.lastSelectedListID)) {
                        root.lastSelectedListID = root.isVideo ? 2 : 1
                        root.lastSelectedListName = root.isVideo ? "全部视频" : "全部音乐"
                    }

                    playlistView.clearSelection()
                    if(root.playlistModel) root.playlistModel.LoadPlaylists(root.isVideo)

                    Qt.callLater(() => {
                                     root.selectPlaylistById(root.lastSelectedListID)
                                 })
                }
                confirmDialog.open()
            }
        }
    }

    Popup {
        id: textFieldPopup
        parent: Overlay.overlay
        width: parent.width - 40
        height: 24
        padding: 0
        modal: true
        Overlay.modal: Rectangle {
            color: Qt.rgba(0, 0, 0, 0.4)
            Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutSine } }
        }
        background: null
        property bool isRename: false
        property int targetId: -1
        property string targetName: ""
        property Item targetItem: null
        property bool isProcessing: false

        onAboutToShow: {
            isProcessing = false
            if(targetItem) {
                let pos = targetItem.mapToItem(Overlay.overlay, 0, 0)
                x = pos.x; y = pos.y; width = targetItem.width
            }
            textField.text = isRename ? targetName : (root.isVideo ? "新建播单" : "新建歌单")
            textField.forceActiveFocus()
            textField.selectAll()
        }

        TextField {
            id: textField
            anchors.fill: parent; padding: 0; maximumLength: 10; horizontalAlignment: TextInput.AlignHCenter
            color: "white"
            font.pointSize: 9
            selectedTextColor: "white"
            selectionColor: "#0A84FF"
            background: Rectangle {
                anchors.fill: parent
                color: Qt.rgba(50/255, 50/255, 50/255, 1)
                radius: 5
                border.color: Qt.rgba(100/255, 200/255, 255/255, 0.6)
                border.width: 1
            }

            onEditingFinished: {
                if(textFieldPopup.isProcessing) return
                textFieldPopup.isProcessing = true
                textFieldPopup.close()
                if (text.trim().length === 0) return

                if(!textFieldPopup.isRename) {
                    XCPlayer.SavePlaylist(text.trim(), root.isVideo)
                } else if(text.trim() !== textFieldPopup.targetName) {
                    XCPlayer.RenamePlaylist(text.trim(), textFieldPopup.targetId)
                    if(textFieldPopup.targetId === root.lastSelectedListID) {
                        root.lastSelectedListName = text.trim()
                        root.listSelected(root.lastSelectedListID, root.lastSelectedListName)
                    }
                }

                if(root.playlistModel) root.playlistModel.LoadPlaylists(root.isVideo)
            }
        }
    }
}
