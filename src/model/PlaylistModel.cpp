#include "PlaylistModel.h"
#include "manager/DatabaseManager.h"

PlaylistModel::PlaylistModel(QObject *parent)
    : QAbstractListModel{parent}
{}

void PlaylistModel::LoadPlaylists(bool isVideo)
{
    beginResetModel();

    QList<XC::Playlist> playlists = DatabaseManager::GetInstance().LoadPlaylists();
    items.clear();
    items.reserve(playlists.count());
    for(const auto& playlist : playlists) {
        if(playlist.isVideo == isVideo) {
            items.append({
                playlist.name,
                playlist.id,
                playlist.isVideo
            });
        }
    }

    endResetModel();
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || index.row() >= items.size()) return QVariant();

    const auto& item = items[index.row()];
    switch(role) {
    case NameRole:      return item.name;
    case IDRole:        return item.id;
    case IsVideoRole:   return item.isVideo;
    default:            return QVariant();
    }
}
