#ifndef COVERMANAGER_H
#define COVERMANAGER_H

#include <QQuickImageProvider>
#include <QCache>
#include <QPixmap>
#include <QSet>

class CoverManager : public QObject
{
    Q_OBJECT
public:
    CoverManager(const CoverManager&) = delete;
    CoverManager& operator=(const CoverManager&) = delete;

    static CoverManager& GetInstance() {
        static CoverManager inst;
        return inst;
    }

    enum CoverSize {
        LargeCover = 0,
        MediumCover,
        SmallCover,
        SmallFrame
    };

    void Init(const QString& path);

    QPixmap GetCoverSync(int id, const QString& url, CoverSize size);
    bool GetCoverAsync(int id, const QString& url, CoverSize size);

    void SetLargeCover(QPixmap pix) { largeCover = pix; }
    QPixmap GetLargeCover() const { return largeCover; }
    QPixmap GetCachedMediumCover(int id);
    QPixmap GetCachedSmallCover(int id);
    void DeleteCoverCache(const QList<int>& idList);
    // 提取图片前 n 个主色调
    QList<QColor> GetLargeCoverDominantColors(int n);
    QList<QColor> GetLocalImageDominantColors(const QString& localUrl, int n);

private:
    explicit CoverManager(QObject *parent = nullptr);

    void ProcessCover(QImage& cover, CoverSize size);

    QCache<int, QPixmap> smallCache;    // 小图封面缓存
    QCache<int, QPixmap> mediumCache;   // 中图封面缓存
    QPixmap largeCover;                 // 大图
    QString diskCacheDir;               // 本地缓存目录
    QSet<int> loadingSmallSet;          // 正在加载的小图ID
    QSet<int> loadingMediumSet;         // 正在加载的中图ID
    QSet<int> noCoverSet;               // 没有封面的ID

    const QSize largeSize{1024, 1024};
    const QSize mediumSize{240, 240};
    const QSize smallSize{48, 48};
    const QSize frameSize{96, 54};

signals:
    void mediumCoverReady(int id);
    void smallCoverReady(int id);
};

class CoverProvider : public QQuickImageProvider
{
public:
    CoverProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

    QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override {
        QPixmap pix;

        // 移除时间戳参数
        QString cleanId = id.section('?', 0, 0);
        // 提取最后一段数字ID
        QStringList parts = cleanId.split('/');
        int coverID = parts.last().toInt();

        if(id.contains("large")) {
            pix = CoverManager::GetInstance().GetLargeCover();
        } else if(id.contains("medium")) {
            pix = CoverManager::GetInstance().GetCachedMediumCover(coverID);
        } else if(id.contains("small")) {
            pix = CoverManager::GetInstance().GetCachedSmallCover(coverID);
        }

        if(size) *size = pix.size();
        return pix;
    }
};

#endif // COVERMANAGER_H
