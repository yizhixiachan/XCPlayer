#include "AlbumModel.h"
#include "manager/CoverManager.h"
#include "manager/DatabaseManager.h"

AlbumModel::AlbumModel(QObject *parent)
    : QAbstractListModel{parent}
{
    connect(&CoverManager::GetInstance(), &CoverManager::mediumCoverReady,
            this, &AlbumModel::onMediumCoverReady);
}

void AlbumModel::LoadAlbumsFromArtist(const QString &artist)
{
    beginResetModel();

    items = DatabaseManager::GetInstance().LoadAlbumsFromArtist(artist);

    endResetModel();
}

QVariant AlbumModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || index.row() >= items.size()) return QVariant();

    const auto& item = items[index.row()];
    switch(role) {
    case NameRole:  return item.name;
    case CoverRole: {
        bool ok = CoverManager::GetInstance().GetCoverAsync(item.firstID, item.firstUrl, CoverManager::MediumCover);
        if(ok) {
                    return QString("image://covers/medium/%1").arg(item.firstID);
        } else {
                    return QString("qrc:/assets/icons/Audio.png");
        }
    }
    case CountRole: return item.count;
    default:        return QVariant();
    }
}

void AlbumModel::onMediumCoverReady(int id)
{
    for(int i = 0; i < items.size(); ++i) {
        if(items[i].firstID == id) {
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx, {CoverRole});
            break;
        }
    }
}
