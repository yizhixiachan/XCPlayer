import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import XCPlayer

Item {
    id: root
    Component.onCompleted: {
        audioPlaylists.selectPlaylistById(1)
    }

    property PlaylistModel audioPlaylist: null
    property PlaylistModel videoPlaylist: null
    property int pageIndex: 0
    property int maxAvailableListHeight: Math.max(0, root.height - 360)
    property bool isVideo
    property int listID
    property string listName

    function clearPlaylistsSelection() {
        audioPlaylists.clearSelection();
        videoPlaylists.clearSelection();
    }

    function selectPlaylist(isVideo, listID) {
        if(isVideo) {
            videoPlaylists.selectPlaylistById(listID)
        } else {
            audioPlaylists.selectPlaylistById(listID)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 3

        ButtonGroup { id: btnGroup }

        PlaylistView {
            id: audioPlaylists
            Layout.fillWidth: true
            availableHeight: root.maxAvailableListHeight
            isVideo: false
            playlistModel: root.audioPlaylist
            title: "音乐";
            iconSource: "qrc:/assets/icons/Audio.png"
            isExpanded: true
            ButtonGroup.group: btnGroup

            onIsExpandedChanged: if(isExpanded) videoPlaylists.isExpanded = false

            onListSelected: (listID, listName) => {
                                root.listName = listName;
                                root.listID = listID;
                                root.isVideo = false
                                root.pageIndex = 0
                            }
        }

        PlaylistView {
            id: videoPlaylists
            Layout.fillWidth: true
            availableHeight: root.maxAvailableListHeight
            isVideo: true
            playlistModel: root.videoPlaylist
            title: "视频"
            iconSource: "qrc:/assets/icons/Video.png"
            ButtonGroup.group: btnGroup

            onIsExpandedChanged: if(isExpanded) audioPlaylists.isExpanded = false

            onListSelected: (listID, listName) => {
                                root.listName = listName;
                                root.listID = listID;
                                root.isVideo = true
                                root.pageIndex = 0
                            }
        }

        Item { Layout.preferredHeight: 3; }

        NavButton {
            Layout.fillWidth: true
            text: "网络流（Beta）"
            icon.source: "qrc:/assets/icons/Stream.png"
            icon.width: 18; icon.height: 18
            ButtonGroup.group: btnGroup
            onNormalClicked: {
                root.pageIndex = 1
                root.clearPlaylistsSelection();
            }
        }

        Item { Layout.preferredHeight: 5; }

        NavButton {
            Layout.fillWidth: true
            text: "艺术家"
            icon.source: "qrc:/assets/icons/Artist.png"
            icon.width: 18; icon.height: 18
            ButtonGroup.group: btnGroup
            onNormalClicked: {
                root.pageIndex = 2
                root.clearPlaylistsSelection();
            }
        }

        NavButton {
            Layout.fillWidth: true
            text: "媒体库"
            icon.source: "qrc:/assets/icons/Library.png"
            icon.width: 18; icon.height: 18
            ButtonGroup.group: btnGroup
            onNormalClicked: {
                root.pageIndex = 3
                root.clearPlaylistsSelection();
            }
        }

        NavButton {
            Layout.fillWidth: true
            text: "工具箱"
            icon.source: "qrc:/assets/icons/Tool.png"
            icon.width: 18; icon.height: 18
            ButtonGroup.group: btnGroup
            onNormalClicked: {
                root.pageIndex = 4
                root.clearPlaylistsSelection();
            }
        }

        NavButton {
            Layout.fillWidth: true
            text: "设置"
            icon.source: "qrc:/assets/icons/Setting.png"
            icon.width: 18; icon.height: 18
            ButtonGroup.group: btnGroup
            onNormalClicked:{
                root.pageIndex = 5
                root.clearPlaylistsSelection();
            }
        }

        Item { Layout.fillHeight: true; Layout.fillWidth: true }
    }
}
