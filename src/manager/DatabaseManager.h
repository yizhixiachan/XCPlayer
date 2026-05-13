#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QThread>
#include "utility/MediaInfo.h"

class DatabaseManager
{
public:
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    static DatabaseManager& GetInstance() {
        static DatabaseManager inst;
        return inst;
    }

    void Init(const QString& path);

    void SaveBaseInfo(QList<XC::BaseInfo>& infoList, int userListID = -1);
    QList<XC::BaseInfo> LoadBaseInfoFromPlaylist(int listID);
    QList<XC::BaseInfo> LoadBaseInfoFromAlbum(const QString& artist, const QString& album);
    XC::BaseInfo LoadBaseInfoByMediaID(int mediaID);
    void DeleteBaseInfoFromPlaylist(const QList<int>& mediaIDList, int listID = -1);
    void SaveLastPosition(int mediaID, double lastPosition);
    QStringList LoadArtists();
    QList<XC::AlbumInfo> LoadAlbumsFromArtist(const QString& artist);
    QHash<int, QString> LoadUrlsByMediaIDs(const QList<int>& mediaIDList);

    void SavePlaylist(const QString& name, bool isVideo);
    QList<XC::Playlist> LoadPlaylists();
    void DeletePlaylists(const QList<int>& listIDList);
    void AddBaseInfoToPlaylist(const QList<int>& mediaIDList, int userListID);
    void RenamePlaylist(const QString& newName, int userListID);

    int GetMediaCountByType(bool isVideo);
    int GetAlbumCount();
    int GetWatchedVideoCount();
    int GetHDRVideoCount();

private:
    DatabaseManager();
    ~DatabaseManager();

    void Execute(const std::function<void()>& task);

    QString dbPath;

    QThread dbThread;
    // 数据库线程任务接收者
    QObject* dbContext;
    QSqlDatabase db;
};

#endif // DATABASEMANAGER_H
