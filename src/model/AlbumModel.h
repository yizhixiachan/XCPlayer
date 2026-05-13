#ifndef ALBUMMODEL_H
#define ALBUMMODEL_H

#include <QAbstractListModel>
#include <QQmlEngine>
#include "utility/MediaInfo.h"

class AlbumModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit AlbumModel(QObject *parent = nullptr);

    enum AlbumRole {
        NameRole = Qt::UserRole + 1,
        CoverRole,
        CountRole
    };

    Q_INVOKABLE void LoadAlbumsFromArtist(const QString& artist);
protected:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return items.size(); }
    QHash<int, QByteArray> roleNames() const override {
        QHash<int, QByteArray> roles;
        roles[NameRole] = "name";
        roles[CoverRole] = "cover";
        roles[CountRole] = "count";
        return roles;
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    void onMediumCoverReady(int id);

    QList<XC::AlbumInfo> items;
};

#endif // ALBUMMODEL_H
