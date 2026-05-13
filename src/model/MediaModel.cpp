#include "MediaModel.h"
#include "XCPlayer.h"
#include "manager/CoverManager.h"
#include "manager/DatabaseManager.h"

MediaModel::MediaModel(QObject *parent)
    : QAbstractListModel{parent}
{
    connect(&XCPlayer::GetInstance(), &XCPlayer::lastPositionUpdated,
            this, &MediaModel::onLastPositionUpdated);
    connect(&CoverManager::GetInstance(), &CoverManager::smallCoverReady,
            this, &MediaModel::onSmallCoverReady);
}

void MediaModel::LoadBaseInfoFromPlaylist(int listID)
{
    beginResetModel();

    items = DatabaseManager::GetInstance().LoadBaseInfoFromPlaylist(listID);
    idSet.clear();
    for(const auto& item : items) {
        idSet.insert(item.id);
    }

    endResetModel();
}

void MediaModel::LoadBaseInfoFromAlbum(const QString &artist, const QString &album)
{
    beginResetModel();

    items = DatabaseManager::GetInstance().LoadBaseInfoFromAlbum(artist, album);
    idSet.clear();
    for(const auto& item : items) {
        idSet.insert(item.id);
    }

    endResetModel();
}

void MediaModel::AppendBaseInfo(const QList<XC::BaseInfo> &infoList, bool isVideo)
{
    QList<XC::BaseInfo> appendItems;
    for(const auto& item : infoList) {
        if(item.isVideo == isVideo && !idSet.contains(item.id)) {
            appendItems.append(item);
            idSet.insert(item.id);
        }
    }

    if(appendItems.isEmpty()) return;

    beginInsertRows(QModelIndex(), items.size(), items.size() + appendItems.size() - 1);

    items.append(appendItems);

    endInsertRows();
}

void MediaModel::DeleteBaseInfo(const QList<int> &mediaIDList, int listID)
{

    emit beforeDataDeleted(mediaIDList, listID);

    QSet<int> deletedIDSet(mediaIDList.begin(), mediaIDList.end());
    QList<int> deletedRows;

    for(int i = 0; i < items.size(); ++i) {
        if(deletedIDSet.contains(items[i].id)) {
            deletedRows.append(i);
        }
    }

    // 降序排序，从大到小按序删除，避免影响待删索引值发生改变
    std::sort(deletedRows.begin(), deletedRows.end(), std::greater<int>());

    for(int row : deletedRows) {
        int deletedID = items[row].id;
        beginRemoveRows(QModelIndex(), row, row);

        idSet.remove(deletedID);
        items.removeAt(row);

        endRemoveRows();
    }

    // 从数据库中删除
    DatabaseManager::GetInstance().DeleteBaseInfoFromPlaylist(mediaIDList, listID);

    emit afterDataDeleted(listID);
}

void MediaModel::ReloadBaseInfo(int mediaID)
{
     for(int i = 0; i < items.size(); ++i) {
        if(items[i].id == mediaID) {
            // 从数据库重新加载单条最新数据
            items[i] = DatabaseManager::GetInstance().LoadBaseInfoByMediaID(mediaID);
            QModelIndex idx = index(i);
            // 触发数据改变信号
            emit dataChanged(idx, idx);
            break;
        }
    }
}

void MediaModel::InsertBaseInfo(int mediaID)
{
    if(mediaID == -1 || idSet.contains(mediaID)) return;

    XC::BaseInfo info = DatabaseManager::GetInstance().LoadBaseInfoByMediaID(mediaID);
    if (!info.IsValid()) return;

    beginInsertRows(QModelIndex(), items.size(), items.size());

    items.append(info);
    idSet.insert(mediaID);

    endInsertRows();
}

bool MediaModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if(row < 0 || count <= 0 || row + count > items.size()) return false;

    int removedID = items[row].id;

    beginRemoveRows(parent, row, row + count - 1);

    idSet.remove(removedID);
    items.removeAt(row);

    endRemoveRows();

    return true;
}

QVariant MediaModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || index.row() >= items.size()) return QVariant();

    const auto& item = items[index.row()];
    switch(role) {
    case CoverRole: {
        bool ok = CoverManager::GetInstance().GetCoverAsync(item.id, item.url, item.isVideo ? CoverManager::SmallFrame
                                                                                            : CoverManager::SmallCover);
        if(ok) {
                            return QString("image://covers/small/%1").arg(item.id);
        } else {
                            return item.isVideo ? QString("qrc:/assets/icons/Camera.png")
                                                : QString("qrc:/assets/icons/Audio.png");
        }
    }
    case UrlRole:           return item.url;
    case TitleRole:         return item.title;
    case ArtistRole:        return item.artist;
    case AlbumRole:         return item.album;
    case HDRFormatRole:     return item.hdrFormat;
    case DurationRole:      return item.duration;
    case LastPositionRole:  return item.lastPosition;
    case FpsRole:           return item.num / (double)item.den;
    case ResolutionRole:    return QString("%1x%2").arg(item.width).arg(item.height);
    case IDRole:            return item.id;
    case IsVideoRole:       return item.isVideo;
    default:                return QVariant();
    }
}

void MediaModel::onSmallCoverReady(int id)
{
    for(int i = 0; i < items.size(); ++i) {
        if(items[i].id == id) {
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx, {CoverRole});
            break;
        }
    }
}

void MediaModel::onLastPositionUpdated(int mediaID, double lastPosition)
{
    for(int i = 0; i < items.size(); ++i) {
        if(items[i].id == mediaID) {
            items[i].lastPosition = lastPosition;
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx, {LastPositionRole});
            break;
        }
    }
}
