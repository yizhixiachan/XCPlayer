import QtQuick
import QtQuick.Controls

ListView {
    id: root
    clip: true

    focus: true
    activeFocusOnTab: true

    property var selectionMap: ({})
    property int selectedCount: Object.keys(selectionMap).length
    property int anchorIndex: -1    // Shift 多选起始索引锚定

    property int scrollBarTopPadding: 0
    property int scrollBarBottomPadding: 0

    property var isSelectable: function(idx) { return true; }

    // 全选快捷键
    Shortcut {
        sequence: StandardKey.SelectAll
        enabled: root.visible && root.activeFocus
        onActivated: selectAll()
    }

    MouseArea {
        anchors.fill: parent
        z: -1
        onClicked: {
            root.forceActiveFocus()
            root.clearSelection()
        }
    }

    // 多选逻辑
    function handleSelection(clickedIndex, modifiers) {
        // 是否允许被选中
        if(!isSelectable(clickedIndex)) return;

        let newMap = {};

        if(modifiers & Qt.ShiftModifier) {
            // Shift 多选
            if(anchorIndex === -1) anchorIndex = clickedIndex; // 如果没有锚点，则以当前点为起点
            let start = Math.min(anchorIndex, clickedIndex);
            let end = Math.max(anchorIndex, clickedIndex);

            // 如果也按下 Ctrl，继承原有选中
            if(modifiers & Qt.ControlModifier) {
                Object.assign(newMap, selectionMap);
            }

            for(let i = start; i <= end; i++) {
                if(isSelectable(i)) newMap[i] = true;
            }

        } else if(modifiers & Qt.ControlModifier) {
            // Ctrl 多选
            Object.assign(newMap, selectionMap);
            if(newMap[clickedIndex]) {
                delete newMap[clickedIndex];
            } else {
                newMap[clickedIndex] = true;
            }
            anchorIndex = clickedIndex;

        } else {
            // 普通单选
            newMap[clickedIndex] = true;
            anchorIndex = clickedIndex;
        }

        selectionMap = newMap;
    }

    // 判断是否被选中
    function isSelected(idx) {
        return selectionMap[idx] === true;
    }

    function select(idx) {
        if (!isSelectable(idx)) return;
        let newMap = {}
        newMap[idx] = true
        anchorIndex = idx
        selectionMap = newMap
    }

    // 清空选中状态
    function clearSelection() {
        selectionMap = {};
        anchorIndex = -1;
    }

    // 全选
    function selectAll() {
        let newMap = {};
        for(let i = 0; i < root.count; i++) {
            if(isSelectable(i)) {
                newMap[i] = true;
            }
        }
        root.selectionMap = newMap;
    }


    ScrollBar.vertical: ScrollBar {
        id: vScrollBar
        implicitWidth: 10
        minimumSize : 0.03
        topPadding: root.scrollBarTopPadding
        bottomPadding: root.scrollBarBottomPadding

        contentItem: Rectangle {
            radius: width / 2
            color: Qt.rgba(1, 1, 1, 0.4)
            opacity: vScrollBar.active ? 1.0 : 0.0
            Behavior on opacity {
                NumberAnimation { duration: 500; easing.type: Easing.OutSine }
            }
        }
    }
}
