#ifndef XCPLAYER_H
#define XCPLAYER_H

#include <QObject>
#include <QQmlEngine>
#include <QUrl>
#include <QColor>
#include <QVariantList>
#include "utility/MediaInfo.h"
#include "core/XCMediaCore.h"

class QTimer;

class XCPlayer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool isPlaying READ IsPlaying NOTIFY playStateChanged)
    Q_PROPERTY(double masterClock READ GetMasterClock NOTIFY progressChanged)
    Q_PROPERTY(QString timeText READ FormatTime NOTIFY progressChanged)
    Q_PROPERTY(float speed READ GetSpeed NOTIFY speedChanged)
    Q_PROPERTY(float volume READ GetVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool isMute READ IsMute NOTIFY muteChanged)
    Q_PROPERTY(bool isHWEnabled READ IsHWEnabled NOTIFY hwEnabledChanged)

    Q_PROPERTY(XC::BaseInfo playInfo READ GetPlayInfo NOTIFY playInfoChanged)
    Q_PROPERTY(QString cover READ GetCover NOTIFY playInfoChanged)
    Q_PROPERTY(QString largeCover READ GetLargeCover NOTIFY playInfoChanged)
    Q_PROPERTY(int listID READ GetListID NOTIFY listIDChanged)
    Q_PROPERTY(int audioStreamIndex READ GetAudioStreamIndex NOTIFY streamIndexChanged)
    Q_PROPERTY(int videoStreamIndex READ GetVideoStreamIndex NOTIFY streamIndexChanged)
    Q_PROPERTY(int subtitleStreamIndex READ GetSubtitleStreamIndex NOTIFY streamIndexChanged)
    Q_PROPERTY(QVariantList audioStreams READ GetAudioStreams NOTIFY playInfoChanged)
    Q_PROPERTY(QVariantList videoStreams READ GetVideoStreams NOTIFY playInfoChanged)
    Q_PROPERTY(QVariantList subtitleStreams READ GetSubtitleStreams NOTIFY playInfoChanged)
    Q_PROPERTY(QString lyrics READ GetLyrics NOTIFY playInfoChanged)
    Q_PROPERTY(QVariantList chapters READ GetChapters NOTIFY playInfoChanged)

    Q_PROPERTY(bool useExtSubtitle MEMBER useExtSubtitle NOTIFY extSubtitleStateChanged)
    Q_PROPERTY(QString extSubtitleText READ GetExtSubtitleText NOTIFY extSubtitleTextChanged)

    Q_PROPERTY(double bufferPosition READ GetBufferPosition NOTIFY progressChanged)
    Q_PROPERTY(bool isLive READ IsLive NOTIFY playInfoChanged)

public:
    XCPlayer(const XCPlayer&) = delete;
    XCPlayer& operator=(const XCPlayer&) = delete;

    static XCPlayer *create(QQmlEngine *, QJSEngine *) {
        XCPlayer* instance = &GetInstance();
        // C++ 管理生命周期
        QQmlEngine::setObjectOwnership(instance, QQmlEngine::CppOwnership);

        return instance;
    }

    static XCPlayer& GetInstance() {
        static XCPlayer inst;
        return inst;
    }

    bool IsPlaying() const { return core.IsPlaying(); }
    double GetMasterClock() const { return core.GetMasterClock(); }
    float GetSpeed() const { return core.GetSpeed(); }
    float GetVolume() const { return core.GetVolume(); }
    bool IsMute() const { return core.IsMute(); }
    bool IsHWEnabled() const { return core.IsHWEnabled(); }

    XC::BaseInfo GetPlayInfo() const { return playInfo; }
    QString GetCover() const { return cover; }
    QString GetLargeCover() const { return largeCover; }
    int GetListID() const { return listID; }
    int GetAudioStreamIndex() const { return core.GetAudioStreamIndex(); }
    int GetVideoStreamIndex() const { return core.GetVideoStreamIndex(); }
    int GetSubtitleStreamIndex() const { return core.GetSubtitleStreamIndex(); }
    QVariantList GetAudioStreams() const;
    QVariantList GetVideoStreams() const;
    QVariantList GetSubtitleStreams() const;
    QString GetLyrics() const;
    QVariantList GetChapters() const;

    QString GetExtSubtitleText() const { return currentExtSubText; }

    double GetBufferPosition() const { return core.GetLatestPacketPts(); }

    bool IsLive() const { return core.IsLive(); }

    QString FormatTime() {
        if(IsLive()) {
            return FormatDuration(GetMasterClock(), true) + " / " + FormatDuration(GetBufferPosition(), true);
        }
        int h = playInfo.duration / 3600;
        return FormatDuration(GetMasterClock(), h > 0) + " / " + FormatDuration(playInfo.duration, false);
    }

    Q_INVOKABLE QVariantMap CheckMetadataCapabilities(const QString& url);
    Q_INVOKABLE QString GetUrlByMediaID(int mediaID);
    Q_INVOKABLE QVariantMap GetLibraryStats();
    Q_INVOKABLE void Reset();

    void Quit();

public slots:
    // 启动自检
    void StartChecking();
    // 导入媒体文件
    void ProcessUrls(const QList<QUrl>& urls, int listID);
    // 保存歌单
    void SavePlaylist(const QString& name, bool isVideo);
    // 删除歌单
    void DeletePlaylists(const QList<int>& listIDList);
    // 添加媒体到歌单
    void AddBaseInfoToPlaylist(const QList<int>& mediaIDList, int userListID);
    // 重命名歌单
    void RenamePlaylist(const QString& newName, int userListID);
    // 加载所有艺术家
    QStringList LoadArtists();

    // 加载媒体信详细信息
    void LoadMediaInfoAsync(int mediaID);
    // 加载容器元数据
    void LoadMetadataAsync(int mediaID);
    // 修改容器元数据
    void ModifyMetadataAsync(int mediaID, const QVariantMap& metadataMap);
    // 修改封面
    void ReplaceCoverAsync(int mediaID, const QUrl& coverUrl);
    // 导入外部字幕
    void LoadExternalSubtitleAsync(const QUrl& url);

    // 使用自定义图片背景
    void SetCustomImageBackground(const QString& imageUrl);
    // 保存封面
    void SaveCover(int mediaID, const QUrl& saveUrl);

    // 导入LRC文件
    QVariantList MatchLRCFiles(const QList<QUrl>& lrcUrls);
    void WriteLRCFiles(const QVariantList& matchData);


    // --- 播放控制 ---
    void PlayTempUrl(const QString& url, const QVariantMap& options);
    void Play(int mediaID, int listID);
    void SetPause(bool pause);
    void Seek(double sec) { core.Seek(sec); if(useExtSubtitle) UpdateExternalSubtitle(sec); }
    void SetSpeed(double multiplier) { core.SetSpeed(multiplier); emit speedChanged(); }
    void OpenAudioStream(int index) { core.OpenAudioStream(index); emit streamIndexChanged(); }
    void OpenVideoStream(int index) { core.OpenVideoStream(index); emit streamIndexChanged(); }
    void OpenSubtitleStream(int index) { core.OpenSubtitleStream(index); emit streamIndexChanged(); }
    void SetVolume(float value) { core.SetVolume(value); emit volumeChanged(); }
    void SetMute(bool mute) { core.SetMute(mute); emit muteChanged(); }
    void SetExclusiveMode(bool exclusive) { core.SetExclusiveMode(exclusive); }
    void SetReplayGainEnabled(bool enable) { core.SetReplayGainEnabled(enable); }
    void SetHWAccelEnabled(bool enable) { core.SetHWAccelEnabled(enable); emit hwEnabledChanged(); }
    void SetVideoRenderer(IVideoRenderer* renderer) { core.SetVideoRenderer(renderer); }
    void SetD3D11Device(void* device) { core.SetD3D11Device(device); }

    // 获取图片前 n 个主导色
    QList<QColor> GetLargeCoverDominantColors(int n);
    QList<QColor> GetLocalImageDominantColors(const QString& imageUrl, int n);

    // 打开文件所在位置
    void ShowInExplorer(int mediaID);
    // 读取 LRC 文件内容
    QString ReadLRCFile(const QUrl& fileUrl);
    // 格式化时间
    QString FormatDuration(int sec, bool forceHHMMSS = false);

private:
    explicit XCPlayer(QObject *parent = nullptr);

    void UpdatePlayInfo(int mediaID, int listID);
    void UpdateExternalSubtitle(double sec);
    QVariantMap GetMediaInfo(const QString& url, AVDictionary* opts = nullptr);

    static bool CheckSuffix(const QString& suffix);
    static QStringList ScanDir(const QString& dirPath, bool recursive, std::atomic<int>& scannedCount);
    static void UpdateTaskbarProgress(int current, int total, bool isFinished = false);

    XCMediaCore core;
    QTimer* progressTimer{nullptr};

    // 播放中的媒体数据
    XC::BaseInfo playInfo;
    XC::MediaInfo mediaInfo;
    QString cover{"qrc:/assets/icons/Audio.png"};
    QString largeCover{"qrc:/assets/icons/Audio.png"};
    int listID{1};


    // 外挂字幕
    QString currentExtSubText;
    QList<XC::ExternalSubtitle> extSubtitles;
    bool useExtSubtitle{false};

    QVariantMap lastStreamOptions;

    // 播放请求保护
    std::mutex playMutex;
    std::atomic<int> playRequestSerial{0};

    std::atomic<bool> bProcessing{false};
public:
    std::atomic<bool> bGlobalStopReq{false};

signals:
    void filesMissed(const QStringList& missingFiles);
    void busyRequest(QString msg);
    void chunkReady(const QList<XC::BaseInfo>& chunk, int listID);
    void processUrlsFinished(QList<QString> failedUrls, bool success, QString msg);

    void mediaInfoReady(QVariantMap infoMap);
    void metadataReady(int mediaID, QVariantMap metadata, QString imageUrl);

    void modifyFinished(int mediaID, bool success, QString msg);
    void replaceFinished(bool success, QString msg);

    void customImageSaved(QString url, bool success, QString msg);
    void finished(bool success, QString msg);

    void playCompleted();
    void progressChanged();
    void speedChanged();
    void volumeChanged();
    void muteChanged();
    void hwEnabledChanged();
    void playStateChanged();
    void playInfoChanged();
    void listIDChanged();
    void streamIndexChanged();

    void lastPositionUpdated(int mediaID, double lastPosition);

    void extSubtitleStateChanged();
    void extSubtitleTextChanged();
};

#endif // XCPLAYER_H
