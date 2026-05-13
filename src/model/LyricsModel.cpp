#include "LyricsModel.h"
#include "utility/XCFL.h"

LyricsModel::LyricsModel(QObject *parent)
    : QAbstractListModel{parent}
{}

void LyricsModel::LoadLyrics(const QString &lyrics)
{

    beginResetModel();

    auto lyricsMap = XCFL::FormatLyrics(lyrics);
    items.clear();
    currIndex = -1;
    items.reserve(lyricsMap.count());
    for(const auto& line : lyricsMap.toStdMap()) {
        items.append({line.second.first, line.second.second, line.first});
    }

    endResetModel();

    emit currentLineChanged();
}

bool LyricsModel::UpdateCurrentIndex(double sec)
{
    if(items.isEmpty()) return false;

    // 检查是否需要更新
    if(currIndex >= 0 && currIndex < items.size()) {
        double currLineTime = items[currIndex].time;
        double nextLineTime = (currIndex + 1 < items.size())
                                  ? items[currIndex + 1].time : 1e9;
        if(sec >= currLineTime && sec < nextLineTime) return false;
    }

    // 二分查找目标行
    auto it = std::upper_bound(items.begin(), items.end(), sec,
                               [](double sec, const LyricLine& line) {
                                   return sec < line.time;
                               });

    // 获取当前行歌词位置
    int targetRow = std::distance(items.begin(), it) - 1;

    // 更新并触发信号
    if(currIndex != targetRow) {
        int oldIndex = currIndex;
        currIndex = targetRow;

        if(oldIndex != -1) {
            // 通知旧行不在是当前行
            emit dataChanged(index(oldIndex), index(oldIndex), {IsCurrentRole});
        }

        // 通知新行是当前行
        emit dataChanged(index(currIndex), index(currIndex), {IsCurrentRole});

        // 通知桌面歌词当前行变化
        emit currentLineChanged();

        return true;
    }

    return false;
}

double LyricsModel::GetTimeAt(int index) const
{
    if(index >= 0 && index < items.size()) {
        return items[index].time;
    }
    return 0.0;
}

QVariant LyricsModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || index.row() >= items.size()) return QVariant();

    const auto& item = items[index.row()];
    switch(role) {
    case OriginalRole:      return item.original;
    case TranslationRole:   return item.translation;
    case IsCurrentRole:     return (index.row() == currIndex);
    default:                return QVariant();
    }
}
