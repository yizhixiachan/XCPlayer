#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QAbstractListModel>
#include <QQmlEngine>
#include "utility/MediaInfo.h"

class PlaylistModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit PlaylistModel(QObject *parent = nullptr);

    enum PlaylistRole {
        NameRole = Qt::UserRole + 1,
        IDRole,
        IsVideoRole
    };

    Q_INVOKABLE void LoadPlaylists(bool isVideo);

protected:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return items.size(); }
    QHash<int, QByteArray> roleNames() const override {
        QHash<int, QByteArray> roles;
        roles[NameRole] = "name";
        roles[IDRole] = "listID";
        roles[IsVideoRole] = "isVideo";
        return roles;
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    QList<XC::Playlist> items;
};

#endif // PLAYLISTMODEL_H
