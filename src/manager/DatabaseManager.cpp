#include "DatabaseManager.h"
#include "CoverManager.h"
#include <QSqlQuery>

DatabaseManager::DatabaseManager()
{
    dbContext = new QObject();

    // 把对象移到数据库线程
    dbContext->moveToThread(&dbThread);
    dbThread.start();
}

DatabaseManager::~DatabaseManager()
{
    Execute([this]() {
        if(db.isOpen()) {
            db.close();
        }
        QSqlDatabase::removeDatabase("GlobalSingleDB");
    });

    dbThread.quit();
    dbThread.wait();

    delete dbContext;
}

void DatabaseManager::Init(const QString &path)
{
    dbPath = path;

    Execute([this]() {
        db = QSqlDatabase::addDatabase("QSQLITE", "GlobalSingleDB");
        db.setDatabaseName(dbPath);

        if(!db.open()) return;

        // 配置 SQLite
        QSqlQuery query(db);
        query.exec("PRAGMA journal_mode=WAL;");
        query.exec("PRAGMA synchronous=NORMAL;");
        query.exec("PRAGMA foreign_keys=ON;");
        query.exec("PRAGMA temp_store=MEMORY;");
        query.exec("PRAGMA cache_size=-10000;");

        db.transaction();

        // 创建媒体表
        query.exec(R"(CREATE TABLE IF NOT EXISTS media (
            id INTEGER PRIMARY KEY, isVideo INTEGER, url TEXT UNIQUE,
            title TEXT, artist TEXT, album TEXT, hdrFormat TEXT, duration REAL,
            lastPosition REAL, num INTEGER, den INTEGER, width INTEGER, height INTEGER))");

        // 创建歌单表
        query.exec(R"(CREATE TABLE IF NOT EXISTS playlist (
            id INTEGER PRIMARY KEY, isVideo INTEGER, name TEXT))");

        // 创建歌单映射表
        query.exec(R"(CREATE TABLE IF NOT EXISTS playlist_media_map (
            playlist_id INTEGER, media_id INTEGER,
            PRIMARY KEY (playlist_id, media_id),
            FOREIGN KEY (playlist_id) REFERENCES playlist(id) ON DELETE CASCADE,
            FOREIGN KEY (media_id) REFERENCES media(id) ON DELETE CASCADE))");

        query.exec("CREATE INDEX IF NOT EXISTS idx_pmm_media_id ON playlist_media_map(media_id)");

        query.exec("INSERT OR IGNORE INTO playlist (id, isVideo, name) VALUES (1, 0, '全部音乐')");
        query.exec("INSERT OR IGNORE INTO playlist (id, isVideo, name) VALUES (2, 1, '全部视频')");

        db.commit();
    });
}

void DatabaseManager::SaveBaseInfo(QList<XC::BaseInfo> &infoList, int userListID)
{
    if(infoList.isEmpty()) return;

    Execute([&]() {
        db.transaction();
        QSqlQuery query(db);

        // 批量插入媒体表
        query.prepare(R"(
            INSERT INTO media (isVideo, url, title, artist, album, hdrFormat, duration, lastPosition, num, den, width, height)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(url) DO UPDATE SET
                isVideo=excluded.isVideo, title=excluded.title, artist=excluded.artist, album=excluded.album,
                hdrFormat=excluded.hdrFormat, duration=excluded.duration, lastPosition=excluded.lastPosition,
                num=excluded.num, den=excluded.den, width=excluded.width, height=excluded.height
        )");

        const int count = infoList.size();
        QVariantList isVideoList, urlList, titleList, artistList, albumList, hdrFormatList,
            durationList, lastPositionList, numList, denList, widthList, heightList;
        isVideoList.reserve(count); urlList.reserve(count); titleList.reserve(count);
        artistList.reserve(count); albumList.reserve(count); hdrFormatList.reserve(count);
        durationList.reserve(count); lastPositionList.reserve(count);
        numList.reserve(count); denList.reserve(count); widthList.reserve(count); heightList.reserve(count);

        for(const auto& info : infoList) {
            isVideoList << (info.isVideo ? 1 : 0); urlList << info.url; titleList << info.title;
            artistList << info.artist; albumList << info.album; hdrFormatList << info.hdrFormat;
            durationList << info.duration; lastPositionList << info.lastPosition;
            numList << info.num; denList << info.den; widthList << info.width; heightList << info.height;
        }

        query.addBindValue(isVideoList); query.addBindValue(urlList); query.addBindValue(titleList);
        query.addBindValue(artistList); query.addBindValue(albumList); query.addBindValue(hdrFormatList);
        query.addBindValue(durationList); query.addBindValue(lastPositionList);
        query.addBindValue(numList); query.addBindValue(denList); query.addBindValue(widthList); query.addBindValue(heightList);

        if(!query.execBatch()) {
            db.rollback();
            return;
        }

        // 查回真实ID并同步给 infoList
        QStringList urls;
        QHash<QString, XC::BaseInfo*> urlMap;
        urlMap.reserve(count);
        urls.reserve(count);

        for(auto& info : infoList) {
            urlMap[info.url] = &info;
            urls << info.url;
        }

        QVariantList mapPlaylistIds, mapMediaIds;
        mapPlaylistIds.reserve(count * 2); mapMediaIds.reserve(count * 2);

        bool isCustomListValid = false;
        bool customListIsVideo = false;
        if(userListID >= 3) {
            QSqlQuery typeQuery(db);
            typeQuery.prepare("SELECT isVideo FROM playlist WHERE id = ?");
            typeQuery.addBindValue(userListID);
            if (typeQuery.exec() && typeQuery.next()) {
                isCustomListValid = true;
                customListIsVideo = typeQuery.value(0).toInt() == 1;
            }
        }

        const int CHUNK_LIMIT = 900;
        for(int i = 0; i < urls.size(); i += CHUNK_LIMIT) {
            QStringList chunkUrls = urls.mid(i, CHUNK_LIMIT);
            QStringList placeholders;
            placeholders.reserve(chunkUrls.size());
            for (int j = 0; j < chunkUrls.size(); ++j) placeholders << "?";

            QString sqlStr = QString("SELECT id, isVideo, url FROM media WHERE url IN (%1)").arg(placeholders.join(","));
            QSqlQuery selectQuery(db);
            selectQuery.prepare(sqlStr);

            for (const QString& url : chunkUrls) selectQuery.addBindValue(url);

            selectQuery.exec();
            while (selectQuery.next()) {
                int mediaID = selectQuery.value(0).toInt();
                bool isVideo = selectQuery.value(1).toInt() == 1;
                QString url = selectQuery.value(2).toString();

                if (urlMap.contains(url)) urlMap[url]->id = mediaID;

                mapPlaylistIds << (isVideo ? 2 : 1);
                mapMediaIds << mediaID;

                if (userListID >= 3 && isCustomListValid && (isVideo == customListIsVideo)) {
                    mapPlaylistIds << userListID;
                    mapMediaIds << mediaID;
                }
            }
        }

        // 批量写入歌单映射表
        QSqlQuery mapQuery(db);
        mapQuery.prepare("INSERT OR IGNORE INTO playlist_media_map (playlist_id, media_id) VALUES (?, ?)");
        mapQuery.addBindValue(mapPlaylistIds);
        mapQuery.addBindValue(mapMediaIds);

        if (!mapQuery.execBatch()) {
            db.rollback();
            return;
        }

        db.commit();
    });
}

QList<XC::BaseInfo> DatabaseManager::LoadBaseInfoFromPlaylist(int listID)
{
    QList<XC::BaseInfo> results;

    Execute([&]() {
        QSqlQuery query(db);
        query.prepare(R"(
            SELECT m.* FROM media m
            INNER JOIN playlist_media_map pm ON m.id = pm.media_id
            WHERE pm.playlist_id = ?
        )");
        query.addBindValue(listID);

        if(query.exec()) {
            while(query.next()) {
                XC::BaseInfo info;
                info.id = query.value("id").toInt();
                info.isVideo = query.value("isVideo").toInt() == 1;
                info.url = query.value("url").toString();
                info.title = query.value("title").toString();
                info.artist = query.value("artist").toString();
                info.album = query.value("album").toString();
                info.hdrFormat = query.value("hdrFormat").toString();
                info.duration = query.value("duration").toDouble();
                info.lastPosition = query.value("lastPosition").toDouble();
                info.num = query.value("num").toInt();
                info.den = query.value("den").toInt();
                info.width = query.value("width").toInt();
                info.height = query.value("height").toInt();
                results.emplaceBack(info);
            }
        }
    });

    return results;
}

QList<XC::BaseInfo> DatabaseManager::LoadBaseInfoFromAlbum(const QString& artist, const QString& album)
{
    QList<XC::BaseInfo> results;

    Execute([&]() {
        QSqlQuery query(db);
        query.prepare("SELECT * FROM media WHERE ('/' || artist || '/' LIKE '%/' || ? || '/%') AND album = ? ORDER BY title ASC");
        query.addBindValue(artist);
        query.addBindValue(album);

        if(query.exec()) {
            while(query.next()) {
                XC::BaseInfo info;
                info.id = query.value("id").toInt();
                info.isVideo = false;
                info.url = query.value("url").toString();
                info.title = query.value("title").toString();
                info.artist = query.value("artist").toString();
                info.album = query.value("album").toString();
                info.duration = query.value("duration").toDouble();
                results.emplaceBack(info);
            }
        }
    });

    return results;
}

XC::BaseInfo DatabaseManager::LoadBaseInfoByMediaID(int mediaID)
{
    XC::BaseInfo result{};

    Execute([&]() {
        QSqlQuery query(db);
        query.prepare("SELECT * FROM media WHERE id = ?");
        query.addBindValue(mediaID);

        if(query.exec() && query.next()) {
            result.id = query.value("id").toInt();
            result.isVideo = query.value("isVideo").toInt() == 1;
            result.url = query.value("url").toString();
            result.title = query.value("title").toString();
            result.artist = query.value("artist").toString();
            result.album = query.value("album").toString();
            result.hdrFormat = query.value("hdrFormat").toString();
            result.duration = query.value("duration").toDouble();
            result.lastPosition = query.value("lastPosition").toDouble();
            result.num = query.value("num").toInt();
            result.den = query.value("den").toInt();
            result.width = query.value("width").toInt();
            result.height = query.value("height").toInt();
        }
    });

    return result;
}

void DatabaseManager::DeleteBaseInfoFromPlaylist(const QList<int> &mediaIDList, int listID)
{
    if(mediaIDList.isEmpty()) return;

    Execute([&]() {
        db.transaction();
        QSqlQuery query(db);

        QVariantList bindIds;
        bindIds.reserve(mediaIDList.size());
        for(int id : mediaIDList) bindIds << id;

        if(listID == -1 || listID == 1 || listID == 2) {
            query.prepare("DELETE FROM media WHERE id = ?");
            query.addBindValue(bindIds);

            QList<int> idList = mediaIDList;
            QMetaObject::invokeMethod(&CoverManager::GetInstance(), [idList]() {
                CoverManager::GetInstance().DeleteCoverCache(idList);
            }, Qt::QueuedConnection);

        } else {
            query.prepare("DELETE FROM playlist_media_map WHERE playlist_id = ? AND media_id = ?");
            QVariantList bindListIds;
            bindListIds.reserve(mediaIDList.size());
            for (int i = 0; i < mediaIDList.size(); ++i) bindListIds << listID;

            query.addBindValue(bindListIds);
            query.addBindValue(bindIds);
        }

        if(!query.execBatch()) {
            db.rollback();
            return;
        }
        db.commit();
    });
}

void DatabaseManager::SaveLastPosition(int mediaID, double lastPosition)
{
    Execute([&]() {
        QSqlQuery query(db);
        query.prepare("UPDATE media SET lastPosition = ? WHERE id = ?");
        query.addBindValue(lastPosition);
        query.addBindValue(mediaID);
        query.exec();
    });
}

QStringList DatabaseManager::LoadArtists()
{
    QStringList results;

    Execute([&]() {
        QSqlQuery query(db);
        query.prepare(R"(
            SELECT DISTINCT artist
            FROM media
            WHERE isVideo = 0 AND artist IS NOT NULL AND artist != ''
        )");

        if(query.exec()) {
            QSet<QString> artistSet;

            while(query.next()) {
                QString rawArtist = query.value(0).toString();
                // 按 "/" 拆分歌手
                QStringList splitArtists = rawArtist.split("/", Qt::SkipEmptyParts);
                for (const QString& singleArtist : splitArtists) {
                    QString trimmedArtist = singleArtist.trimmed();
                    if (!trimmedArtist.isEmpty()) {
                        artistSet.insert(trimmedArtist);
                    }
                }
            }

            results = QStringList(artistSet.begin(), artistSet.end());
            results.sort(Qt::CaseInsensitive);
        }
    });

    return results;
}

QList<XC::AlbumInfo> DatabaseManager::LoadAlbumsFromArtist(const QString &artist)
{
    QList<XC::AlbumInfo> results;

    Execute([&]() {
        QSqlQuery query(db);
        query.prepare(R"(
            SELECT album, id, url, COUNT(id), MIN(title)
            FROM media
            WHERE ('/' || artist || '/') LIKE '%/' || ? || '/%'
              AND album IS NOT NULL AND album != ''
            GROUP BY album
            ORDER BY album ASC
        )");

        query.addBindValue(artist);

        if(query.exec()) {
            while(query.next()) {
                XC::AlbumInfo info;
                info.name = query.value(0).toString();      // 专辑名
                info.firstID = query.value(1).toInt();      // 专辑中第一个媒体ID
                info.firstUrl = query.value(2).toString();  // 专辑中第一个媒体Url
                info.count = query.value(3).toInt();        // 专辑内的歌曲数量
                results.append(info);
            }
        }
    });

    return results;
}

QHash<int, QString> DatabaseManager::LoadUrlsByMediaIDs(const QList<int> &mediaIDList)
{
    QHash<int, QString> results;
    if (mediaIDList.isEmpty()) return results;

    Execute([&]() {
        QSqlQuery query(db);

        const int CHUNK_LIMIT = 900;
        for(int i = 0; i < mediaIDList.size(); i += CHUNK_LIMIT) {
            QList<int> chunk = mediaIDList.mid(i, CHUNK_LIMIT);

            QStringList placeholders;
            placeholders.reserve(chunk.size());
            for(int j = 0; j < chunk.size(); ++j) placeholders << "?";

            QString sqlStr = QString("SELECT id, url FROM media WHERE id IN (%1)").arg(placeholders.join(","));
            query.prepare(sqlStr);

            for(int id : chunk) {
                query.addBindValue(id);
            }

            if(query.exec()) {
                while (query.next()) {
                    results.insert(query.value(0).toInt(), query.value(1).toString());
                }
            }
        }
    });

    return results;
}

void DatabaseManager::SavePlaylist(const QString &name, bool isVideo)
{
    if(name.trimmed().isEmpty()) return;

    Execute([&]() {
        QSqlQuery query(db);
        query.prepare("INSERT INTO playlist (isVideo, name) VALUES (?, ?)");
        query.addBindValue(isVideo ? 1 : 0);
        query.addBindValue(name.trimmed());
        query.exec();
    });
}

QList<XC::Playlist> DatabaseManager::LoadPlaylists()
{
    QList<XC::Playlist> results;

    Execute([&]() {
        QSqlQuery query(db);
        if(query.exec("SELECT id, isVideo, name FROM playlist ORDER BY id ASC")) {
            while(query.next()) {
                XC::Playlist p;
                p.id = query.value("id").toInt();
                p.isVideo = query.value("isVideo").toInt() == 1;
                p.name = query.value("name").toString();
                results.emplaceBack(p);
            }
        }
    });

    return results;
}

void DatabaseManager::DeletePlaylists(const QList<int> &listIDList)
{
    if (listIDList.isEmpty()) return;

    Execute([&]() {
        db.transaction();
        QSqlQuery query(db);
        query.prepare("DELETE FROM playlist WHERE id = ?");

        QVariantList bindIds;
        bindIds.reserve(listIDList.size());
        for(int id : listIDList) {
            if (id >= 3) bindIds << id;
        }

        if(bindIds.isEmpty()) {
            db.rollback();
            return;
        }

        query.addBindValue(bindIds);
        if(!query.execBatch()) {
            db.rollback();
            return;
        }
        db.commit();
    });
}

void DatabaseManager::AddBaseInfoToPlaylist(const QList<int> &mediaIDList, int userListID)
{
    if(mediaIDList.isEmpty() || userListID <= 2) return;

    Execute([&]() {
        db.transaction();
        QSqlQuery query(db);
        query.prepare("INSERT OR IGNORE INTO playlist_media_map (playlist_id, media_id) VALUES (?, ?)");

        QVariantList bindPlaylistIds, bindMediaIds;
        bindPlaylistIds.reserve(mediaIDList.size());
        bindMediaIds.reserve(mediaIDList.size());

        for(int mediaId : mediaIDList) {
            bindPlaylistIds << userListID;
            bindMediaIds << mediaId;
        }

        query.addBindValue(bindPlaylistIds);
        query.addBindValue(bindMediaIds);

        if(!query.execBatch()) {
            db.rollback();
            return;
        }
        db.commit();
    });
}

void DatabaseManager::RenamePlaylist(const QString &newName, int userListID)
{
    if(userListID <= 2 || newName.trimmed().isEmpty()) return;

    Execute([&]() {
        QSqlQuery query(db);
        query.prepare("UPDATE playlist SET name = ? WHERE id = ?");
        query.addBindValue(newName.trimmed());
        query.addBindValue(userListID);
        query.exec();
    });
}

int DatabaseManager::GetMediaCountByType(bool isVideo)
{
    int count = 0;
    Execute([&]() {
        QSqlQuery query(db);
        query.prepare("SELECT COUNT(id) FROM media WHERE isVideo = ?");
        if(isVideo) {
            query.addBindValue(1);
        } else {
            query.addBindValue(0);
        }

        if(query.exec() && query.next()) {
            count = query.value(0).toInt();
        }
    });
    return count;
}

int DatabaseManager::GetAlbumCount()
{
    int count = 0;
    Execute([&]() {
        QSqlQuery query(db);
        if(query.exec("SELECT COUNT(DISTINCT album) FROM media WHERE album IS NOT NULL AND album != ''") && query.next()) {
            count = query.value(0).toInt();
        }
    });
    return count;
}

int DatabaseManager::GetWatchedVideoCount()
{
    int count = 0;
    Execute([&]() {
        QSqlQuery query(db);
        if(query.exec(R"(SELECT COUNT(id) FROM media
                            WHERE isVideo = 1 AND
                                duration > 0 AND
                                ROUND(lastPosition / duration * 100) >= 99)") && query.next()) {
            count = query.value(0).toInt();
        }
    });
    return count;
}

int DatabaseManager::GetHDRVideoCount()
{
    int count = 0;
    Execute([&]() {
        QSqlQuery query(db);
        if(query.exec("SELECT COUNT(id) FROM media WHERE isVideo = 1 AND hdrFormat IS NOT NULL AND hdrFormat != '' AND LOWER(hdrFormat) != 'sdr'")
            && query.next()) {
            count = query.value(0).toInt();
        }
    });
    return count;
}

void DatabaseManager::Execute(const std::function<void()>& task)
{
    if(!dbThread.isRunning()) return;

    if(QThread::currentThread() == &dbThread) {
        task();
    } else {
        // 跨线程任务，BlockingQueuedConnection 阻塞数据库线程直到完成
        QMetaObject::invokeMethod(dbContext, task, Qt::BlockingQueuedConnection);
    }
}
