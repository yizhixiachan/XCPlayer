#ifndef MEDIAMODEL_H
#define MEDIAMODEL_H

#include <QAbstractListModel>
#include <QQmlEngine>
#include "utility/MediaInfo.h"

class MediaModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit MediaModel(QObject *parent = nullptr);

    enum MediaRole {
        CoverRole = Qt::UserRole + 1,
        UrlRole,
        TitleRole,
        ArtistRole,
        AlbumRole,
        HDRFormatRole,
        DurationRole,
        LastPositionRole,
        FpsRole,
        ResolutionRole,
        IDRole,
        IsVideoRole
    }; Q_ENUM(MediaRole)

    Q_INVOKABLE void LoadBaseInfoFromPlaylist(int listID);
    Q_INVOKABLE void LoadBaseInfoFromAlbum(const QString& artist, const QString& album);
    Q_INVOKABLE void AppendBaseInfo(const QList<XC::BaseInfo>& infoList, bool isVideo);
    Q_INVOKABLE void DeleteBaseInfo(const QList<int>& mediaIDList, int listID);
    Q_INVOKABLE void ReloadBaseInfo(int mediaID);
    Q_INVOKABLE void InsertBaseInfo(int mediaID);

protected:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return items.size(); }
    QHash<int, QByteArray> roleNames() const override {
        QHash<int, QByteArray> roles;
        roles[CoverRole] = "cover";
        roles[UrlRole] = "url";
        roles[TitleRole] = "title";
        roles[ArtistRole] = "artist";
        roles[AlbumRole] = "album";
        roles[HDRFormatRole] = "hdrFormat";
        roles[DurationRole] = "duration";
        roles[LastPositionRole] = "lastPosition";
        roles[FpsRole] = "fps";
        roles[ResolutionRole] = "resolution";
        roles[IDRole] = "id";
        roles[IsVideoRole] = "isVideo";
        return roles;
    }

    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    void onSmallCoverReady(int id);
    void onLastPositionUpdated(int mediaID, double lastPosition);

    QList<XC::BaseInfo> items;
    QSet<int> idSet;

signals:
    void beforeDataDeleted(const QList<int>& mediaIDList, int listID);
    void afterDataDeleted(int listID);
};

#endif // MEDIAMODEL_H
