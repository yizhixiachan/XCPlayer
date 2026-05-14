#include "XCPlayer.h"
#include "utility/ThreadPool.h"
#include "utility/MediaTool.h"
#include "utility/XCFL.h"
#include "manager/DatabaseManager.h"
#include "manager/CoverManager.h"
#include <QDirIterator>
#include <QTimer>
#include <shobjidl.h>
#include <QGuiApplication>
#include <QWindow>
#include <QProcess>
#include <QStandardPaths>
#include <fstream>

XCPlayer::XCPlayer(QObject *parent)
    : QObject{parent}
    , core{[this]{
        QMetaObject::invokeMethod(this,[this] {
            emit playCompleted();
        }, Qt::QueuedConnection);
    }}
{
    progressTimer = new QTimer(this);
    progressTimer->setInterval(50);
    connect(progressTimer, &QTimer::timeout, this, [this]() {
        emit progressChanged();
        if(useExtSubtitle && !extSubtitles.isEmpty()) {
            UpdateExternalSubtitle(GetMasterClock());
        }
    });
}

QVariantList XCPlayer::GetAudioStreams() const
{
    QVariantList list;
    for(const auto& stream : mediaInfo.audioStreams) {
        QVariantMap map;
        map["index"] = stream.index;
        map["language"] = stream.language;
        list.append(map);
    }
    return list;
}

QVariantList XCPlayer::GetVideoStreams() const
{
    QVariantList list;
    for(const auto& stream : mediaInfo.videoStreams) {
        if(stream.isAttachedPic) continue;
        QVariantMap map;
        map["index"] = stream.index;
        map["language"] = stream.language;
        list.append(map);
    }
    return list;
}

QVariantList XCPlayer::GetSubtitleStreams() const
{
    QVariantList list;
    for(const auto& stream : mediaInfo.subtitleStreams) {
        QVariantMap map;
        map["index"] = stream.index;
        map["language"] = stream.language;
        list.append(map);
    }
    return list;
}

QString XCPlayer::GetLyrics() const
{
    QStringList candidateKeys = {"lyrics", "lyrics-xxx", "lyric"};

    // 容器元数据中查找
    for(auto it = mediaInfo.metadata.constBegin(); it != mediaInfo.metadata.constEnd(); ++it) {
        for(const QString& key : candidateKeys) {
            if(it.key().compare(key, Qt::CaseInsensitive) == 0) {
                return it.value();
            }
        }
    }

    // 音频流元数据中查找
    for (const auto& stream : mediaInfo.audioStreams) {
        for (auto it = stream.metadata.constBegin(); it != stream.metadata.constEnd(); ++it) {
            for (const QString& key : candidateKeys) {
                if (it.key().compare(key, Qt::CaseInsensitive) == 0) {
                    return it.value();
                }
            }
        }
    }

    return "";
}

QVariantList XCPlayer::GetChapters() const
{
    QVariantList list;
    for (const auto& ch : mediaInfo.chapters) {
        QVariantMap map;
        map["startTime"] = ch.startTime;
        map["endTime"] = ch.endTime;
        map["title"] = ch.title;
        list.append(map);
    }
    return list;
}

QVariantMap XCPlayer::CheckMetadataCapabilities(const QString &url)
{
    QVariantMap res;
    QString suffix = QFileInfo(QUrl(url).toLocalFile()).suffix().toLower();
    if(suffix.isEmpty()) suffix = QFileInfo(url).suffix().toLower();

    // 不支持写入元数据的格式
    static const QSet<QString> noMeta = {"aac", "ac3", "amr", "dts", "ape"};

    // 不支持写入内嵌歌词的格式
    static const QSet<QString> noLyrics = {"wav"};

    // 不支持写入内嵌封面的格式
    static const QSet<QString> noCover = {"wav"};

    if(noMeta.contains(suffix)) {
        res["canEdit"] = false;
        res["canEditLyrics"] = false;
        res["canReplaceCover"] = false;
    } else {
        res["canEdit"] = true;
        res["canEditLyrics"] = !noLyrics.contains(suffix);
        res["canReplaceCover"] = !noCover.contains(suffix);
    }

    return res;
}

QString XCPlayer::GetUrlByMediaID(int mediaID)
{
    return DatabaseManager::GetInstance().LoadUrlsByMediaIDs(QList<int>{mediaID}).value(mediaID, "");
}

QVariantMap XCPlayer::GetLibraryStats()
{
    QVariantMap stats;
    int audioCount = DatabaseManager::GetInstance().GetMediaCountByType(false);
    int artistCount = DatabaseManager::GetInstance().LoadArtists().count();
    int albumCount = DatabaseManager::GetInstance().GetAlbumCount();
    int videoCount = DatabaseManager::GetInstance().GetMediaCountByType(true);
    int watchedVideoCount = DatabaseManager::GetInstance().GetWatchedVideoCount();
    int hdrVideoCount = DatabaseManager::GetInstance().GetHDRVideoCount();
    stats["audioCount"] = audioCount;
    stats["artistCount"] = artistCount;
    stats["albumCount"] = albumCount;
    stats["videoCount"] = videoCount;
    stats["watchedVideoCount"] = watchedVideoCount;
    stats["hdrVideoCount"] = hdrVideoCount;

    return stats;
}

void XCPlayer::Reset()
{
    core.Stop();

    playInfo = XC::BaseInfo{};
    cover = playInfo.isVideo ? "qrc:/assets/icons/Camera.png" : "qrc:/assets/icons/Audio.png";
    largeCover = playInfo.isVideo ? "qrc:/assets/icons/Camera.png" : "qrc:/assets/icons/Audio.png";

    mediaInfo = XC::MediaInfo{};

    emit playInfoChanged();
    emit playStateChanged();
}

void XCPlayer::Quit()
{
    bGlobalStopReq.store(true, std::memory_order_release);

    core.Abort();

    if(playInfo.id != -1 && playInfo.isVideo) {
        int lastPosition = GetMasterClock();
        DatabaseManager::GetInstance().SaveLastPosition(playInfo.id, lastPosition);
    }
}

void XCPlayer::StartChecking()
{
    QList<XC::BaseInfo> allMedia = DatabaseManager::GetInstance().LoadBaseInfoFromPlaylist(1);
    allMedia.append(DatabaseManager::GetInstance().LoadBaseInfoFromPlaylist(2));

    ThreadPool::GetInstance().PushTask([this, allMedia]() {
        QList<int> missingIDList;
        QStringList missingUrls;

        for(const auto& info : allMedia) {
            if(!QFileInfo::exists(info.url)) {
                missingIDList.append(info.id);
                missingUrls.append(info.url);
            }
        }

        if(!missingIDList.isEmpty()) {
            QMetaObject::invokeMethod(this, [this, missingUrls, missingIDList]() {
                // 从数据库中删除
                DatabaseManager::GetInstance().DeleteBaseInfoFromPlaylist(missingIDList);
                emit filesMissed(missingUrls);
            }, Qt::QueuedConnection);
        }
    });
}

void XCPlayer::ProcessUrls(const QList<QUrl> &urls, int listID)
{
    if(urls.isEmpty() || bProcessing.exchange(true, std::memory_order_acquire)) {
        emit busyRequest("后台忙碌中，请稍后再试...");
        return;
    }

    UpdateTaskbarProgress(0, 0);
    emit busyRequest("深度扫描中... 已找到 0 个文件");

    ThreadPool::GetInstance().PushTask([this, urls, listID]() {
        // 文件扫描
        std::atomic<int> scannedCount{0};
        QStringList mediaUrls;
        std::vector<std::future<QStringList>> urlfutures;

        for(const auto& url : urls) {
            if(bGlobalStopReq.load(std::memory_order_acquire)) break;

            if(!url.isLocalFile()) continue;

            QString path = url.toLocalFile();
            QFileInfo fileInfo(path);

            if(!fileInfo.exists()) continue;

            if(fileInfo.isFile() && CheckSuffix(fileInfo.suffix())) {
                mediaUrls.append(path);
                int current = scannedCount.fetch_add(1, std::memory_order_relaxed) + 1;
                QMetaObject::invokeMethod(this, [this, current] {
                    emit busyRequest("深度扫描中... 已找到 " + QString::number(current) + " 个文件");
                }, Qt::QueuedConnection);

            } else if(fileInfo.isDir()) {
                // 扫描当前目录下的文件
                mediaUrls.append(ScanDir(path, false, scannedCount));

                // 子目录递归扫描
                QDir root(path);
                QStringList subDirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for(const auto &sub : subDirs) {
                    QString subPath = root.filePath(sub);
                    urlfutures.emplace_back(ThreadPool::GetInstance().PushTask_WithFuture(&XCPlayer::ScanDir, subPath, true, std::ref(scannedCount)));
                }
            }
        }

        // 阻塞等待所有扫描任务结束
        for(auto& future : urlfutures) {
            if(future.valid()) {
                mediaUrls.append(future.get());
            }
        }

        if(bGlobalStopReq.load(std::memory_order_acquire)) {
            bProcessing.store(false, std::memory_order_release);
            return;
        }

        int total = mediaUrls.size();
        if(total == 0) {
            QMetaObject::invokeMethod(this,[this] {
                UpdateTaskbarProgress(0, 0, true);
                emit processUrlsFinished({}, false, "未扫描到媒体文件！");
            }, Qt::QueuedConnection);
            bProcessing.store(false, std::memory_order_release);
            return;
        }

        QMetaObject::invokeMethod(this,[this, total] {
            UpdateTaskbarProgress(0, total);
            emit busyRequest("正在导入文件... 0 / " + QString::number(total));
        }, Qt::QueuedConnection);


        // 提取信息
        std::atomic<int> completedCount{0};
        std::mutex writeMutex;
        QList<XC::BaseInfo> infoChunk;
        QStringList failedUrls;

        std::vector<std::future<void>> voidFutures;

        for(const QString& url : mediaUrls) {
            if(bGlobalStopReq.load(std::memory_order_acquire)) break;

            voidFutures.emplace_back(ThreadPool::GetInstance().PushTask_WithFuture(
                [this, url, listID, total, &completedCount, &writeMutex, &infoChunk, &failedUrls]() {
                    if(bGlobalStopReq.load(std::memory_order_acquire)) return;

                    XC::BaseInfo info = MediaTool::ExtractBaseInfo(url, bGlobalStopReq);

                    if(bGlobalStopReq.load(std::memory_order_acquire)) return;

                    int current = completedCount.fetch_add(1, std::memory_order_relaxed) + 1;
                    QList<XC::BaseInfo> chunkToImport;

                    // 临界区
                    {
                        std::lock_guard<std::mutex> lock(writeMutex);
                        if(info.IsValid()) {
                            infoChunk.append(info);
                        } else {
                            failedUrls.append(url);
                        }

                        if (infoChunk.size() >= 100) {
                            chunkToImport = infoChunk;
                            infoChunk.clear();
                        }
                    }

                    // 分批入库
                    if(!chunkToImport.isEmpty()) {
                        QMetaObject::invokeMethod(this, [this, chunk = std::move(chunkToImport), listID]() mutable {
                            DatabaseManager::GetInstance().SaveBaseInfo(chunk, listID);
                            emit chunkReady(chunk, listID);
                        }, Qt::QueuedConnection);
                    }

                    QMetaObject::invokeMethod(this,[this, current, total] {
                        UpdateTaskbarProgress(current, total);
                        emit busyRequest("正在导入文件... " + QString::number(current) + " / " + QString::number(total));
                    }, Qt::QueuedConnection);
                }));
        }

        // 阻塞等待所有导入任务
        for(auto& future : voidFutures) {
            if(future.valid()) future.get();
        }

        if(bGlobalStopReq.load(std::memory_order_acquire)) {
            bProcessing.store(false, std::memory_order_release);
            return;
        }

        // 处理最后一批剩余数据
        if(!infoChunk.isEmpty()) {
            QMetaObject::invokeMethod(this, [this, chunk = std::move(infoChunk), listID]() mutable {
                DatabaseManager::GetInstance().SaveBaseInfo(chunk, listID);
                emit chunkReady(chunk, listID);
            }, Qt::QueuedConnection);
        }

        QMetaObject::invokeMethod(this,[this, total, failed = std::move(failedUrls)]() {
            UpdateTaskbarProgress(0, 0, true);
            int successCount = total - failed.size();
            QString msg = successCount > 0 ? "成功导入 " + QString::number(successCount) + " 个媒体文件！"
                                           : QString::number(failed.size()) + " 个媒体文件导入失败！";

            emit processUrlsFinished(failed, successCount > 0, msg);
        }, Qt::QueuedConnection);

        bProcessing.store(false, std::memory_order_release);
    });
}

void XCPlayer::SavePlaylist(const QString &name, bool isVideo)
{
    DatabaseManager::GetInstance().SavePlaylist(name, isVideo);
}

void XCPlayer::DeletePlaylists(const QList<int> &listIDList)
{
    DatabaseManager::GetInstance().DeletePlaylists(listIDList);
}

void XCPlayer::AddBaseInfoToPlaylist(const QList<int> &mediaIDList, int userListID)
{
    DatabaseManager::GetInstance().AddBaseInfoToPlaylist(mediaIDList, userListID);
}

void XCPlayer::RenamePlaylist(const QString &newName, int userListID)
{
    DatabaseManager::GetInstance().RenamePlaylist(newName, userListID);
}

QStringList XCPlayer::LoadArtists()
{
    return DatabaseManager::GetInstance().LoadArtists();
}

void XCPlayer::LoadMediaInfoAsync(int mediaID)
{
    QString url;
    QVariantMap currentOpts;

    // 如果为 -1，取当前正播放的直链的配置
    if(mediaID == -1) {
        url = playInfo.url;
        currentOpts = lastStreamOptions;
    } else {
        url = GetUrlByMediaID(mediaID);
    }

    if(url.isEmpty()) return;

    ThreadPool::GetInstance().PushTask([this, url, currentOpts]() {
        AVDictionary* opts = nullptr;
        for (auto it = currentOpts.constBegin(); it != currentOpts.constEnd(); ++it) {
            QString key = it.key();
            QString val = it.value().toString();
            if (!val.isEmpty()) {
                if (key == "headers" && !val.endsWith("\r\n")) {
                    val += "\r\n";
                }
                av_dict_set(&opts, key.toUtf8().constData(), val.toUtf8().constData(), 0);
            }
        }


        QVariantMap map = GetMediaInfo(url, opts);

        QMetaObject::invokeMethod(this, [this, map]() {
            emit mediaInfoReady(map);
        }, Qt::QueuedConnection);
    });
}

void XCPlayer::LoadMetadataAsync(int mediaID)
{
    QString url = GetUrlByMediaID(mediaID);
    QPixmap pix = CoverManager::GetInstance().GetCoverSync(mediaID, url, CoverManager::MediumCover);
    QString imageUrl = pix.isNull() ? QString("qrc:/assets/icons/Audio.png")
                                    : QString("image://covers/medium/%1?t=%2").arg(QString::number(mediaID),
                                                                                   QString::number(QDateTime::currentMSecsSinceEpoch()));
    ThreadPool::GetInstance().PushTask([this, url, mediaID, imageUrl] {
        XC::MediaInfo info = MediaTool::ExtractMediaInfo(url, bGlobalStopReq);
        QVariantMap map;

        // 获取元数据值
        auto getFuzzy = [&](const QStringList& keys) -> QString {
            // 容器
            for(auto it = info.metadata.constBegin(); it != info.metadata.constEnd(); ++it) {
                for(const auto& k : keys) {
                    if(it.key().compare(k, Qt::CaseInsensitive) == 0) return it.value();
                }
            }
            // 音频流
            for(const auto& stream : info.audioStreams) {
                for(auto it = stream.metadata.constBegin(); it != stream.metadata.constEnd(); ++it) {
                    for(const auto& k : keys) {
                        if(it.key().compare(k, Qt::CaseInsensitive) == 0) return it.value();
                    }
                }
            }
            return QString("");
        };

        map["title"]  = getFuzzy({"title"});
        map["artist"] = getFuzzy({"artist"});
        map["album"]  = getFuzzy({"album"});
        map["lyrics"] = getFuzzy({"lyrics", "lyrics-xxx", "lyric"});

        QMetaObject::invokeMethod(this, [this, mediaID, map, imageUrl]() {
            emit metadataReady(mediaID, map, imageUrl);
        }, Qt::QueuedConnection);
    });
}

void XCPlayer::ModifyMetadataAsync(int mediaID, const QVariantMap &metadataMap)
{
    emit busyRequest("元数据写入中...");
    QString inputUrl = GetUrlByMediaID(mediaID);
    QMap<QString, QString> map;
    for(auto it = metadataMap.constBegin(); it != metadataMap.constEnd(); ++it) {
        map.insert(it.key(), it.value().toString());
    }

    ThreadPool::GetInstance().PushTask([this, mediaID, inputUrl, map] {
        QString error;
        bool success = MediaTool::ModifyMetadata(inputUrl, inputUrl, map, error, bGlobalStopReq);
        QString msg = success ? "元数据写入成功！" : "元数据写入失败！" + error;

        QMetaObject::invokeMethod(this, [this, mediaID, success, msg, inputUrl]() mutable{
            // 更新数据库
            if(success) {
                XC::BaseInfo newBaseInfo = MediaTool::ExtractBaseInfo(inputUrl, bGlobalStopReq);
                newBaseInfo.id = mediaID;
                QList<XC::BaseInfo> infoList{newBaseInfo};
                DatabaseManager::GetInstance().SaveBaseInfo(infoList);
            }
            emit modifyFinished(mediaID, success, msg);
        }, Qt::QueuedConnection);
    });
}

void XCPlayer::ReplaceCoverAsync(int mediaID, const QUrl &coverUrl)
{
    if(!coverUrl.isLocalFile()) return;

    emit busyRequest("封面写入中...");

    QString inputUrl = GetUrlByMediaID(mediaID);
    QString imageUrl = coverUrl.toLocalFile();

    ThreadPool::GetInstance().PushTask([this, mediaID, inputUrl, imageUrl] {
        QString error;
        bool success = MediaTool::ReplaceCover(inputUrl, inputUrl, imageUrl, error, bGlobalStopReq);
        QString msg = success ? "封面写入成功！" : "封面写入失败！" + error;

        QMetaObject::invokeMethod(this, [this, mediaID, inputUrl, success, msg]() {
            // 删除旧的封面缓存
            if(success) {
                CoverManager::GetInstance().DeleteCoverCache({mediaID});
            }
            CoverManager::GetInstance().GetCoverSync(mediaID, inputUrl, CoverManager::MediumCover);
            emit replaceFinished(success, msg);
        }, Qt::QueuedConnection);
    });
}

void XCPlayer::LoadExternalSubtitleAsync(const QUrl& url)
{
    extSubtitles.clear();
    currentExtSubText.clear();
    emit extSubtitleTextChanged();

    if(!url.isLocalFile()) return;
    emit busyRequest("字幕文件解析中...");
    QString localPath = url.toLocalFile();

    ThreadPool::GetInstance().PushTask([this, localPath]() {
        extSubtitles = XCFL::ParseSubtitle(localPath);

        QMetaObject::invokeMethod(this, [this]() {
            QString msg;
            if(extSubtitles.isEmpty()) {
                useExtSubtitle = false;
                msg = "解析字幕文件失败！";
            } else {
                useExtSubtitle = true;
                msg = "解析字幕文件成功！";
                UpdateExternalSubtitle(GetMasterClock());
            }
            emit finished(useExtSubtitle, msg);
            emit extSubtitleStateChanged();
        }, Qt::QueuedConnection);
    });
}

void XCPlayer::SetCustomImageBackground(const QString &imageUrl)
{
    QString savePath;
    bool success{false};
    QString msg;
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir dir(dataDir);
    QStringList filters;
    filters << "bg.*";
    foreach (const QString &oldFile, dir.entryList(filters, QDir::Files)) {
        dir.remove(oldFile);
    }

    if(imageUrl.startsWith("image://covers/large/")) {      // 媒体封面
        savePath = dataDir + "/bg.png";
        int id = imageUrl.section('/', -1).toInt();
        XC::BaseInfo info = DatabaseManager::GetInstance().LoadBaseInfoByMediaID(id);
        QPixmap cover = CoverManager::GetInstance().GetCoverSync(id, info.url, CoverManager::LargeCover);
        if(!cover.isNull()) {
            success = cover.save(savePath, "PNG");
        }
    } else {                                                // 本地图片
        QUrl url(imageUrl);
        if(url.isLocalFile()) {
            QString localFilePath = url.toLocalFile();
            QString suffix = QFileInfo(localFilePath).suffix();
            if(suffix.isEmpty()) suffix = "png";
            savePath = dataDir + "/bg." + suffix;
            success = QFile::copy(localFilePath, savePath);
        }
    }

    msg = success ? "设置自定义背景成功！" : "设置自定义背景失败！";

    emit customImageSaved("file:///" + savePath, success, msg);
}

void XCPlayer::SaveCover(int mediaID, const QUrl &saveUrl)
{
    if(!saveUrl.isLocalFile()) return;

    QString inputUrl = GetUrlByMediaID(mediaID);
    QString savePath = saveUrl.toLocalFile();

    QImage cover = MediaTool::ExtractEmbeddedCover(inputUrl, bGlobalStopReq);
    bool success = false;
    QString msg;

    if(!cover.isNull()) {
        success = cover.save(savePath);
        msg = success ? "封面导出成功！" : "封面导出失败！";
    } else {
        msg = "该文件没有内嵌封面！";
    }

    emit finished(success, msg);
}

QVariantList XCPlayer::MatchLRCFiles(const QList<QUrl> &lrcUrls)
{
    QVariantList result;
    QList<XC::BaseInfo> allAudio = DatabaseManager::GetInstance().LoadBaseInfoFromPlaylist(1);

    for(const QUrl& url : lrcUrls) {
        if(!url.isLocalFile()) continue;
        QString filePath = url.toLocalFile();
        QFileInfo fi(filePath);
        QString baseName = fi.completeBaseName().toLower().trimmed();

        for(const auto& info : allAudio) {
            if(info.isVideo) continue;
            QString title = info.title.toLower().trimmed();
            QString artist = info.artist.toLower().trimmed();
            bool isMatch = false;

            // 标题完全匹配
            if(baseName == title) {
                isMatch = true;
            }
            // 标题和艺术家完全匹配
            else if((baseName == QString("%1 - %2").arg(artist, title) ||
                      baseName == QString("%1 - %2").arg(title, artist))) {
                isMatch = true;
            }
            // 标题和艺术家模糊匹配
            else if(baseName.contains(title) && baseName.contains(artist)) {
                isMatch = true;
            }
            // 标题模糊匹配
            // else if(baseName.contains(title) && baseName.contains("-")) {
            //     isMatch = true;
            // }

            if(isMatch) {
                QVariantMap map;
                map["id"] = info.id;
                map["lrcUrl"] = filePath;
                map["lrcName"] = fi.fileName();
                map["title"] = info.title;
                map["artist"] = info.artist;
                map["selected"] = true;
                result.append(map);
            }
        }
    }
    return result;
}

void XCPlayer::WriteLRCFiles(const QVariantList& matchData)
{
    emit busyRequest("歌词写入中...");

    QList<int> selectedIDs;
    for(const QVariant& item : matchData) {
        if(item.toMap()["selected"].toBool()) {
            selectedIDs.append(item.toMap()["id"].toInt());
        }
    }

    if(selectedIDs.isEmpty()) {
        emit processUrlsFinished({}, false, "未扫描到媒体文件！");
        return;
    }

    QHash<int, QString> urlMap = DatabaseManager::GetInstance().LoadUrlsByMediaIDs(selectedIDs);

    struct LrcTask {
        int mediaID;
        QString lrcContent;
        QString inputUrl;
    };

    QList<LrcTask> validTasks;
    QStringList failedUrls;

    for(const QVariant& item : matchData) {
        QVariantMap map = item.toMap();
        if(!map["selected"].toBool()) continue;

        int mediaID = map["id"].toInt();
        QString lrcPath = map["lrcUrl"].toString();
        QString lrcContent = ReadLRCFile(QUrl::fromLocalFile(lrcPath));

        if(lrcContent.isEmpty()) {
            failedUrls.append(QString("%1 [读取本地LRC文件失败]").arg(lrcPath));
            continue;
        }

        QString inputUrl = urlMap.value(mediaID);
        if(inputUrl.isEmpty()) {
            failedUrls.append(QString("ID: %1 [数据库中未找到该媒体文件]").arg(mediaID));
            continue;
        }

        if(inputUrl.startsWith("file:///")) {
            inputUrl = QUrl(inputUrl).toLocalFile();
        }

        validTasks.append({mediaID, lrcContent, inputUrl});
    }

    ThreadPool::GetInstance().PushTask([this, validTasks, failedUrls]() mutable {
        int successCount = 0;
        QList<XC::BaseInfo> successfulInfos;

        for(const auto& task : validTasks) {
            QMap<QString, QString> qmap;
            qmap.insert("lyrics", task.lrcContent);
            QString error;
            bool success = MediaTool::ModifyMetadata(task.inputUrl, task.inputUrl, qmap, error, bGlobalStopReq);
            if(success) {
                XC::BaseInfo newBaseInfo = MediaTool::ExtractBaseInfo(task.inputUrl, bGlobalStopReq);
                newBaseInfo.id = task.mediaID;
                successfulInfos.append(newBaseInfo);
                successCount++;
            } else {
                failedUrls.append(QString("%1 [%2]").arg(task.inputUrl, error));
            }
        }

        QMetaObject::invokeMethod(this, [this, successfulInfos, successCount, failedUrls]() mutable {
            if(!successfulInfos.isEmpty()) {
                DatabaseManager::GetInstance().SaveBaseInfo(successfulInfos);
            }
            QString msg = successCount > 0 ? "成功写入 " + QString::number(successCount) + " 个LRC文件！"
                                           : QString::number(failedUrls.size()) + " 个LRC文件写入失败！";

            emit processUrlsFinished(failedUrls, successCount > 0, msg);

        }, Qt::QueuedConnection);
    });
}

void XCPlayer::PlayTempUrl(const QString &url, const QVariantMap &options)
{
    if(url.trimmed().isEmpty()) return;

    // 保存上个视频个播放进度
    if(playInfo.id != -1 && playInfo.isVideo) {
        DatabaseManager::GetInstance().SaveLastPosition(playInfo.id, GetMasterClock());
        emit lastPositionUpdated(playInfo.id, GetMasterClock());
    }

    core.Abort();

    // 重置外挂字幕
    if(useExtSubtitle || !extSubtitles.isEmpty() || !currentExtSubText.isEmpty()) {
        useExtSubtitle = false;
        extSubtitles.clear();
        currentExtSubText.clear();
        emit extSubtitleStateChanged();
        emit extSubtitleTextChanged();
    }

    // 临时播放无 listID
    listID = -1;
    emit listIDChanged();

    lastStreamOptions = options;

    // 发生新的播放请求，丢弃旧的播放请求
    int currentSerial = ++playRequestSerial;

    // 重置播放信息
    Reset();

    AVDictionary* opts = nullptr;

    for(auto it = options.constBegin(); it != options.constEnd(); ++it) {
        QString key = it.key();
        QString val = it.value().toString().trimmed();

        if(val.isEmpty()) continue;

        if(key == "headers") {
            val = val.replace("\r\n", "\n").replace("\r", "\n");

            QStringList lines = val.split('\n', Qt::SkipEmptyParts);
            QStringList cleanedLines;

            for(const QString& line : lines) {
                QString trimmedLine = line.trimmed();
                if(!trimmedLine.isEmpty()) {
                    cleanedLines.append(trimmedLine);
                }
            }

            val = cleanedLines.join("\r\n") + "\r\n";
        }
    }

    QString busyMsg;
    QString finalUrl = url;
    QString lowerUrl = url.toLower().trimmed();
    QUrl qUrl(url);

    QString path = qUrl.path().toLower();
    bool isLocal = qUrl.isLocalFile();

    if(!isLocal) {
        busyMsg = "正在连接服务器...";

        av_dict_set(&opts, "rw_timeout", "5000000", 0);

        bool isRtmp = lowerUrl.startsWith("rtmp");
        bool isHlsOrDash = path.endsWith(".m3u8") || path.endsWith(".mpd");
        bool isFlv = path.endsWith(".flv");

        if (isRtmp) {
            // RTMP 直播
            finalUrl = "async:" + finalUrl;
            av_dict_set(&opts, "max_client_capacity", "10485760", 0);

        } else if (isHlsOrDash) {
            // M3U8/MPD 切片列表
            av_dict_set(&opts, "reconnect", "1", 0);
            av_dict_set(&opts, "reconnect_streamed", "1", 0);
            av_dict_set(&opts, "reconnect_delay_max", "5", 0);
            av_dict_set(&opts, "timeout", "5000000", 0);
            av_dict_set(&opts, "buffer_size", "10485760", 0);

        } else if (isFlv) {
            // 直播流，用 async 内存缓冲
            finalUrl = "async:" + finalUrl;
            av_dict_set(&opts, "max_client_capacity", "10485760", 0);

            av_dict_set(&opts, "reconnect", "1", 0);
            av_dict_set(&opts, "reconnect_streamed", "1", 0);
            av_dict_set(&opts, "reconnect_delay_max", "5", 0);
            av_dict_set(&opts, "timeout", "5000000", 0);

        } else if (lowerUrl.startsWith("http")) {
            // 点播流，用 cache 磁盘缓冲，实现随意拖动进度条零延迟
            finalUrl = "cache:" + finalUrl;
            av_dict_set(&opts, "buffer_size", "10485760", 0);

            av_dict_set(&opts, "reconnect", "1", 0);
            av_dict_set(&opts, "reconnect_streamed", "1", 0);
            av_dict_set(&opts, "reconnect_delay_max", "5", 0);
            av_dict_set(&opts, "timeout", "5000000", 0);
        }

    } else {
        busyMsg = "正在读取本地媒体文件...";
        finalUrl = qUrl.toLocalFile();
    }

    emit busyRequest(busyMsg);

    ThreadPool::GetInstance().PushTask([this, finalUrl, opts, currentSerial]() mutable {
        std::lock_guard<std::mutex> lock(playMutex);
        if(playRequestSerial.load(std::memory_order_acquire) != currentSerial) {
            if(opts) av_dict_free(&opts);
            return;
        }

        int ret = core.Play(finalUrl.toStdString(), opts);

        QMetaObject::invokeMethod(this,[this, ret, currentSerial]() {
            if(playRequestSerial.load(std::memory_order_acquire) != currentSerial) {
                return;
            }

            QString message;

            if(ret >= 0) {
                message = "播放成功！";

                playInfo = MediaTool::ExtractBaseInfo(core.GetFormatContext());
                mediaInfo = MediaTool::ExtractMediaInfo(core.GetFormatContext());

                cover = playInfo.isVideo ? "qrc:/assets/icons/Camera.png" : "qrc:/assets/icons/Audio.png";
                largeCover = cover;

                // 选择中文字幕流
                int zhSubtitleIndex = -1;
                for(const auto& st : mediaInfo.subtitleStreams) {
                    if(st.language.contains("中文")) {
                        zhSubtitleIndex = st.index;
                        break;
                    }
                }
                if(zhSubtitleIndex != -1) {
                    core.OpenSubtitleStream(zhSubtitleIndex);
                }

                progressTimer->start();

                emit playInfoChanged();
                emit streamIndexChanged();
                emit playStateChanged();
            } else {
                switch (ret) {
                case -2:
                    message = "请求参数错误！(400 Bad Request)";
                    break;
                case -3:
                    message = "访问未授权！(401 Unauthorized)";
                    break;
                case -4:
                    message = "服务器拒绝访问！(403 Forbidden)";
                    break;
                case -5:
                    message = "资源失效！(404 Not Found)";
                    break;
                case -6:
                    message = "请求过于频繁，请稍后再试！(429 Too Many Requests)";
                    break;
                case -7:
                    message = "客户端请求异常！ (4xx)";
                    break;
                case -1:
                default:
                    message = "播放失败！";
                    break;
                }
            }

            emit finished(ret >= 0, message);
        }, Qt::QueuedConnection);
    });
}

void XCPlayer::Play(int mediaID, int listID)
{
    // 保存上个视频个播放进度
    if(playInfo.id != -1 && playInfo.isVideo) {
        DatabaseManager::GetInstance().SaveLastPosition(playInfo.id, GetMasterClock());
        emit lastPositionUpdated(playInfo.id, GetMasterClock());
    }

    core.Abort();

    // 重置外挂字幕
    if(useExtSubtitle || !extSubtitles.isEmpty() || !currentExtSubText.isEmpty()) {
        useExtSubtitle = false;
        extSubtitles.clear();
        currentExtSubText.clear();
        emit extSubtitleStateChanged();
        emit extSubtitleTextChanged();
    }

    UpdatePlayInfo(mediaID, listID);

    if(playInfo.url.isEmpty()) {
        Reset();
        return;
    }

    // 文件丢失提示
    if(!QFileInfo::exists(playInfo.url)) {
        DatabaseManager::GetInstance().DeleteBaseInfoFromPlaylist({mediaID});
        emit filesMissed({playInfo.url});
        Reset();
        return;
    }

    // 发生新的播放请求，丢弃旧的播放请求
    int currentSerial = ++playRequestSerial;
    AVDictionary* opts = nullptr;

    ThreadPool::GetInstance().PushTask([this, opts, currentSerial]() mutable {
        std::lock_guard<std::mutex> lock(playMutex);
        if(playRequestSerial.load(std::memory_order_acquire) != currentSerial) {
            if(opts) av_dict_free(&opts);
            return;
        }

        int ret = core.Play(playInfo.url.toStdString(), opts);

        QMetaObject::invokeMethod(this,[this, ret, currentSerial]() {
            if(playRequestSerial.load(std::memory_order_acquire) != currentSerial) {
                return;
            }
            if(ret >= 0) {
                mediaInfo = MediaTool::ExtractMediaInfo(core.GetFormatContext());

                // 选择中文字幕流
                int zhSubtitleIndex = -1;
                for(const auto& st : mediaInfo.subtitleStreams) {
                    if(st.language.contains("中文")) {
                        zhSubtitleIndex = st.index;
                        break;
                    }
                }
                if(zhSubtitleIndex != -1) {
                    core.OpenSubtitleStream(zhSubtitleIndex);
                }

                progressTimer->start();
                if(playInfo.lastPosition > 0) {
                    Seek(playInfo.lastPosition - 10);
                }
                emit playInfoChanged();
                emit streamIndexChanged();
                emit playStateChanged();
            } else {
                emit finished(false, "播放失败！");
            }
        }, Qt::QueuedConnection);
    });
}

void XCPlayer::SetPause(bool pause)
{
    if(playInfo.url.isEmpty()) return;

    if(core.IsIdle()) {
        if(playInfo.id != -1) {
            Play(playInfo.id, listID);
        } else {
            return;
        }
    }

    core.SetPause(pause);

    emit playStateChanged();
}

QList<QColor> XCPlayer::GetLargeCoverDominantColors(int n)
{
    return CoverManager::GetInstance().GetLargeCoverDominantColors(n);
}

QList<QColor> XCPlayer::GetLocalImageDominantColors(const QString &imageUrl, int n)
{
    QUrl url(imageUrl);
    return CoverManager::GetInstance().GetLocalImageDominantColors(url.toLocalFile(), n);
}

void XCPlayer::ShowInExplorer(int mediaID)
{
    XC::BaseInfo info = DatabaseManager::GetInstance().LoadBaseInfoByMediaID(mediaID);
    if(info.IsValid()) {
        QString path = QDir::toNativeSeparators(info.url);
        QProcess::startDetached("explorer.exe", {"/select,", path});
    }
}

QString XCPlayer::ReadLRCFile(const QUrl &fileUrl)
{
    if(!fileUrl.isLocalFile()) return "";

    std::filesystem::path p;
#ifdef Q_OS_WIN
    p = fileUrl.toLocalFile().toStdWString();
#else
    p = fileUrl.toLocalFile().toStdString();
#endif

    std::ifstream file(p, std::ios::binary);
    if(!file.is_open()) return "";

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    return XCFL::DecodeText(buffer.str());
}

QString XCPlayer::FormatDuration(int sec, bool forceHHMMSS)
{
    int h = sec / 3600;
    int m = (sec % 3600) / 60;
    int s = sec % 60;

    if (h > 0 || forceHHMMSS) {
        return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2")
        .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
    }
}

void XCPlayer::UpdatePlayInfo(int mediaID, int listID)
{
    if(this->listID != listID) {
        this->listID = listID;
        emit listIDChanged();
    }

    if(mediaID != -1 &&playInfo.id != mediaID) {
        playInfo = DatabaseManager::GetInstance().LoadBaseInfoByMediaID(mediaID);

        QPixmap pix = CoverManager::GetInstance().GetCoverSync(mediaID, playInfo.url, playInfo.isVideo ? CoverManager::SmallFrame
                                                                                                       : CoverManager::SmallCover);
        if(!pix.isNull()) {
            cover = QString("image://covers/small/%1").arg(mediaID);
        } else {
            cover = playInfo.isVideo ? "qrc:/assets/icons/Camera.png" : "qrc:/assets/icons/Audio.png";
        }

        pix = CoverManager::GetInstance().GetCoverSync(mediaID, playInfo.url, CoverManager::LargeCover);
        if(!pix.isNull()) {
            CoverManager::GetInstance().SetLargeCover(pix);
            largeCover = QString("image://covers/large/%1").arg(mediaID);
        } else {
            largeCover = playInfo.isVideo ? "qrc:/assets/icons/Camera.png" : "qrc:/assets/icons/Audio.png";
        }
    }
}

void XCPlayer::UpdateExternalSubtitle(double sec)
{
    if(extSubtitles.isEmpty()) return;

    // 二分查找第一个大于当前时间的字幕
    auto it = std::upper_bound(extSubtitles.begin(), extSubtitles.end(), sec,
                               [](double time, const XC::ExternalSubtitle& sub) {
                                   return time < sub.start;
                               });

    QString newText = "";

    if(it != extSubtitles.begin()) {
        --it; // 回退到上一个已开始的字幕
        // 判断当前时间是否匹配该字幕的时间区间
        if(sec >= it->start && sec <= it->end) {
            newText = it->text;
        }
    }

    if(currentExtSubText != newText) {
        currentExtSubText = newText;
        emit extSubtitleTextChanged();
    }
}

QVariantMap XCPlayer::GetMediaInfo(const QString& url, AVDictionary *opts)
{
    if(url.isEmpty()) {
        if(opts) av_dict_free(&opts);
        return QVariantMap();
    }

    XC::MediaInfo info = MediaTool::ExtractMediaInfo(url, bGlobalStopReq, opts);
    QVariantMap map;

    // 基础容器信息
    map["url"] = info.url;
    map["format"] = info.formatLongName.isEmpty() ? info.format : info.formatLongName;
    map["fileSize"] = (qint64)info.fileSize;
    map["duration"] = info.duration;
    map["bitRate"] = (qint64)info.bitRate;

    auto mapToList = [](const QMap<QString, QString>& dict) {
        QVariantList list;
        for (auto it = dict.begin(); it != dict.end(); ++it) {
            QVariantMap pair;
            pair["key"] = it.key();
            pair["value"] = it.value();
            list.append(pair);
        }
        return list;
    };

    auto extractBaseInfo = [&mapToList](const XC::StreamBaseInfo& stream, QVariantMap& outMap) {
        outMap["index"] = stream.index;
        outMap["codec"] = stream.codecLongName.isEmpty() ? stream.codec : stream.codecLongName;
        outMap["profile"] = stream.profile;
        outMap["bitRate"] = (qint64)stream.bitRate;
        outMap["duration"] = stream.duration;
        outMap["language"] = stream.language;
        outMap["flags"] = stream.flags.join(" | ");
        outMap["sideDataList"] = stream.sideDataList.join(" | ");
        outMap["metadata"] = mapToList(stream.metadata);
    };

    // 容器元数据
    map["metadata"] = mapToList(info.metadata);

    // 音频流信息
    QVariantList audioList;
    for (const auto& a : info.audioStreams) {
        QVariantMap aMap;
        extractBaseInfo(a, aMap);
        aMap["type"] = "Audio";
        aMap["sampleRate"] = a.sampleRate;
        aMap["channels"] = a.channels;
        aMap["channelLayout"] = a.channelLayout;
        aMap["sampleFormat"] = a.sampleFormat;
        aMap["bitDepth"] = a.bitDepth;
        audioList.append(aMap);
    }
    map["audioStreams"] = audioList;

    // 视频流信息
    QVariantList videoList;
    for(const auto& v : info.videoStreams) {
        QVariantMap vMap;
        extractBaseInfo(v, vMap);
        vMap["type"] = "Video";
        vMap["resolution"] = QString("%1x%2").arg(v.width).arg(v.height);
        vMap["fps"] = v.fps;
        vMap["pixelFormat"] = v.pixelFormat;
        vMap["bitDepth"] = v.bitDepth;
        vMap["sar"] = v.sar;
        vMap["dar"] = v.dar;
        vMap["colorRange"] = v.colorRange;
        vMap["colorSpace"] = v.colorSpace;
        vMap["colorTransfer"] = v.colorTransfer;
        vMap["colorPrimaries"] = v.colorPrimaries;
        vMap["hdrFormat"] = v.hdrFormat;
        vMap["rotation"] = v.rotation;
        vMap["isAttachedPic"] = v.isAttachedPic;
        videoList.append(vMap);
    }
    map["videoStreams"] = videoList;

    // 字幕流信息
    QVariantList subList;
    for (const auto& s : info.subtitleStreams) {
        QVariantMap sMap;
        extractBaseInfo(s, sMap);
        sMap["type"] = "Subtitle";
        sMap["isImageBased"] = s.isImageBased;
        if(s.isImageBased) {
            sMap["resolution"] = QString("%1x%2").arg(s.width).arg(s.height);
        }
        subList.append(sMap);
    }
    map["subtitleStreams"] = subList;

    // 章节信息
    QVariantList chapterList;
    for (const auto& ch : info.chapters) {
        QVariantMap chMap;
        chMap["startTime"] = ch.startTime;
        chMap["endTime"] = ch.endTime;
        chMap["title"] = ch.title;
        chapterList.append(chMap);
    }
    map["chapters"] = chapterList;

    return map;
}

bool XCPlayer::CheckSuffix(const QString &suffix)
{
    static const QSet<QString> exts = {
        // ===== 音频格式 =====
        "mp3", "aac", "m4a",
        "flac", "wav", "ape",
        "ogg", "oga", "opus",
        "ac3", "dts", "amr",

        // ===== 视频格式 =====
        "mp4", "mkv", "webm", "avi", "mov", "flv",
        "ogv", "ts", "m2ts", "vob", "mpeg", "mpg",
    };
    return exts.contains(suffix.toLower());
}

QStringList XCPlayer::ScanDir(const QString &dirPath, bool recursive, std::atomic<int>& scannedCount)
{
    QStringList result;
    QDirIterator it(dirPath, QDir::Files | QDir::NoDotAndDotDot,
                    recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);

    while(it.hasNext()) {
        if(XCPlayer::GetInstance().bGlobalStopReq.load(std::memory_order_acquire)) break;
        QString file = it.next();
        if(CheckSuffix(QFileInfo(file).suffix())) {
            result.emplaceBack(file);
            int current = scannedCount.fetch_add(1, std::memory_order_relaxed) + 1;
            if(current % 50 == 0) {
                QMetaObject::invokeMethod(&XCPlayer::GetInstance(), [current] {
                    emit XCPlayer::GetInstance().busyRequest("深度扫描中... 已找到 " + QString::number(current) + " 个文件");
                }, Qt::QueuedConnection);
            }
        }
    }
    return result;
}

void XCPlayer::UpdateTaskbarProgress(int current, int total, bool isFinished) {
    const QWindowList windows = QGuiApplication::topLevelWindows();
    if(windows.isEmpty()) return;

    HWND hwnd = (HWND)windows.constFirst()->winId();
    if(!hwnd) return;

    ITaskbarList3* pTaskbarList = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pTaskbarList));

    if(SUCCEEDED(hr) && pTaskbarList) {
        if(isFinished) {
            // 结束：清除任务栏进度
            pTaskbarList->SetProgressState(hwnd, TBPF_NOPROGRESS);
        } else if(total == 0) {
            // 扫描中：显示暂停的黄色进度
            pTaskbarList->SetProgressState(hwnd, TBPF_PAUSED);
        } else{
            // 导入中：显示正常的绿色进度
            pTaskbarList->SetProgressState(hwnd, TBPF_NORMAL);
            pTaskbarList->SetProgressValue(hwnd, current, total);
        }
        pTaskbarList->Release();
    }
}
