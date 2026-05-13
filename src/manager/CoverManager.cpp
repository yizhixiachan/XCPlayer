#include "CoverManager.h"
#include "utility/MediaTool.h"
#include "utility/XCFL.h"
#include "XCPlayer.h"
#include <QStandardPaths>
#include <QDir>
#include <QPainter>
#include <QThreadPool>

CoverManager::CoverManager(QObject *parent)
    : QObject(parent)
{
    diskCacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir dir;
    if(!dir.exists(diskCacheDir)) dir.mkpath(diskCacheDir);

    // 大约28张300x300 Png图片
    mediumCache.setMaxCost(10 * 1024 * 1024);
    // 大约1000张48x48 Png图片
    smallCache.setMaxCost(10 * 1024 * 1024);
}

QPixmap CoverManager::GetCoverSync(int id, const QString &url, CoverSize size)
{
    if(noCoverSet.contains(id)) return {};
    if(size == MediumCover) {
        if(mediumCache.contains(id)) return GetCachedMediumCover(id);
    } else if(size != LargeCover) {
        if(smallCache.contains(id)) return GetCachedSmallCover(id);
    }

    QImage cover;
    QString diskCacheFile = diskCacheDir + QString("/%1.png").arg(id);

    if((size == SmallCover || size == SmallFrame) && QFile::exists(diskCacheFile)) {
        cover.load(diskCacheFile);
    } else {
        if(size == SmallFrame) {
            cover = MediaTool::ExtractVideoFrame(url, XCPlayer::GetInstance().bGlobalStopReq);
        } else if(size == SmallCover) {
            cover = MediaTool::ExtractEmbeddedCover(url, XCPlayer::GetInstance().bGlobalStopReq);
        } else {
            cover = MediaTool::ExtractEmbeddedCover(url, XCPlayer::GetInstance().bGlobalStopReq);
            if(cover.isNull()) cover = MediaTool::ExtractVideoFrame(url, XCPlayer::GetInstance().bGlobalStopReq);
        }

        if(!cover.isNull()) {
            ProcessCover(cover, size);
            if(size == SmallCover || size == SmallFrame) {
                cover.save(diskCacheFile, "PNG", 100);
            }
        }
    }

    if(cover.isNull()) {
        noCoverSet.insert(id);
        return {};
    } else {
        if(size == MediumCover) {
            mediumCache.insert(id, new QPixmap(QPixmap::fromImage(cover)), cover.sizeInBytes());
        } else if(size != LargeCover) {
            smallCache.insert(id, new QPixmap(QPixmap::fromImage(cover)), cover.sizeInBytes());
        }

        return QPixmap::fromImage(cover);
    }
}

bool CoverManager::GetCoverAsync(int id, const QString &url, CoverSize size)
{
    if(noCoverSet.contains(id)) return false;
    if(size == LargeCover) return false;
    if(size == MediumCover) {
        if(loadingMediumSet.contains(id)) return false;
        if(mediumCache.contains(id)) return true;
        loadingMediumSet.insert(id);
    } else {
        if(loadingSmallSet.contains(id)) return false;
        if(smallCache.contains(id)) return true;
        loadingSmallSet.insert(id);
    }

    QThreadPool::globalInstance()->start([this, id, url, size]() {
        QImage cover;
        QString diskCacheFile = diskCacheDir + QString("/%1.png").arg(id);

        if((size == SmallCover || size == SmallFrame) && QFile::exists(diskCacheFile)) {
            cover.load(diskCacheFile);
        } else {
            if(size == SmallFrame) {
                cover = MediaTool::ExtractVideoFrame(url, XCPlayer::GetInstance().bGlobalStopReq);
            } else {
                cover = MediaTool::ExtractEmbeddedCover(url, XCPlayer::GetInstance().bGlobalStopReq);
            }

            if(!cover.isNull()) {
                ProcessCover(cover, size);
                if(size == SmallCover || size == SmallFrame) {
                    cover.save(diskCacheFile, "PNG", 100);
                }
            }
        }

        // 回到主线程安全修改 QCache QSet
        QMetaObject::invokeMethod(this,[this, id, size, cover] {
            size == MediumCover ? loadingMediumSet.remove(id)
                                : loadingSmallSet.remove(id);
            if(cover.isNull()) {
                noCoverSet.insert(id);
            } else {
                if(size == MediumCover) {
                    mediumCache.insert(id, new QPixmap(QPixmap::fromImage(cover)), cover.sizeInBytes());
                    emit mediumCoverReady(id);
                } else {
                    smallCache.insert(id, new QPixmap(QPixmap::fromImage(cover)), cover.sizeInBytes());
                    emit smallCoverReady(id);
                }
            }
        }, Qt::QueuedConnection);
    });

    return false;
}

QPixmap CoverManager::GetCachedMediumCover(int id)
{
    if(QPixmap* cover = mediumCache[id]) {
        return *cover;
    }

    return {};
}

QPixmap CoverManager::GetCachedSmallCover(int id)
{
    if(QPixmap* cover = smallCache[id]) {
        return *cover;
    }

    QString diskCacheFile = diskCacheDir + QString("/%1.png").arg(id);
    if(QFile::exists(diskCacheFile)) {
        QPixmap cover(diskCacheFile);
        smallCache.insert(id, new QPixmap(cover), cover.toImage().sizeInBytes());
        return cover;
    }

    return {};
}

void CoverManager::DeleteCoverCache(const QList<int> &idList)
{
    for(int id : idList) {
        mediumCache.remove(id);
        smallCache.remove(id);
        loadingMediumSet.remove(id);
        loadingSmallSet.remove(id);
        noCoverSet.remove(id);

        QString diskCacheFile = diskCacheDir + QString("/%1.png").arg(id);
        if(QFile::exists(diskCacheFile)) {
            QFile::remove(diskCacheFile);
        }
        emit mediumCoverReady(id);
        emit smallCoverReady(id);
    }
}

QList<QColor> CoverManager::GetLargeCoverDominantColors(int n)
{
    return XCFL::ExtractDominantColors(largeCover, n);
}

QList<QColor> CoverManager::GetLocalImageDominantColors(const QString &localUrl, int n)
{
    QPixmap img{localUrl};
    if(!img.isNull()) {
        return XCFL::ExtractDominantColors(img, n);
    }
    return {};
}

void CoverManager::ProcessCover(QImage &cover, CoverSize size)
{
    if(cover.isNull()) return;

    QSize scaledSize{48, 48};
    int radius = 3;
    switch(size) {
    case LargeCover:
        scaledSize = largeSize;
        radius = 32;
        break;
    case MediumCover:
        scaledSize = mediumSize;
        radius = 8;
        break;
    case SmallCover:
        scaledSize = smallSize;
        radius = 3;
        break;
    case SmallFrame:
        scaledSize = frameSize;
        break;
    default:
        return;
    }

    if(cover.width() > scaledSize.width() || cover.height() > scaledSize.height()) {
        cover = cover.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    if(size == SmallFrame) return;
    QImage rounded(cover.size(), QImage::Format_ARGB32_Premultiplied);
    rounded.fill(Qt::transparent);
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(QBrush(cover));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rounded.rect(), radius, radius);
    painter.end();
    cover = rounded;
}
