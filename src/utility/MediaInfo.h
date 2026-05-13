#ifndef MEDIAINFO_H
#define MEDIAINFO_H

#include <QMap>
#include <QMetaType>

namespace XC
{
struct Playlist {
    QString name;
    int id{-1};
    bool isVideo{false};
};

struct AlbumInfo {
    QString name;
    QString firstUrl;
    int firstID{-1};
    int count{0};
};

struct BaseInfo {
    Q_GADGET
    Q_PROPERTY(QString url MEMBER url)
    Q_PROPERTY(QString title MEMBER title)
    Q_PROPERTY(QString artist MEMBER artist)
    Q_PROPERTY(QString album MEMBER album)
    Q_PROPERTY(QString hdrFormat MEMBER hdrFormat)
    Q_PROPERTY(double duration MEMBER duration)
    Q_PROPERTY(double lastPosition MEMBER lastPosition)
    Q_PROPERTY(int num MEMBER num)
    Q_PROPERTY(int den MEMBER den)
    Q_PROPERTY(int width MEMBER width)
    Q_PROPERTY(int height MEMBER height)
    Q_PROPERTY(int id MEMBER id)
    Q_PROPERTY(bool isVideo MEMBER isVideo)

public:
    QString url;
    QString title;
    QString artist;
    QString album;
    QString hdrFormat;
    double duration{0.0};
    double lastPosition{0.0}; // 视频专用
    int num{0};
    int den{0};
    int width{0};
    int height{0};
    int id{-1};
    bool isVideo{false};

    bool IsValid() const { return !url.isEmpty(); }
    explicit operator bool() const { return IsValid(); }
    bool operator!() const { return !IsValid(); }
};


// 章节信息
struct ChapterInfo {
    double startTime = 0.0; // 起始时间 (秒)
    double endTime = 0.0;   // 结束时间 (秒)
    QString title;          // 章节标题
};

// 流媒体基础类
struct StreamBaseInfo {
    int index{-1};                      // 流索引
    QString codec;                      // 编解码器名称 (e.g. h264, aac)
    QString codecLongName;              // 编解码器全名
    QString profile;                    // 规格 (e.g. High, Main)
    int64_t bitRate{0};                 // 码率 (bps) ? 为 0
    double duration{0.0};               // 时长 ？为0
    QStringList flags;                  // 状态标志 (Default, Forced, Dub 等)
    QString language;                   // 语言
    QMap<QString, QString> metadata;    // 流级元数据
    QList<QString> sideDataList;        // 附加数据
};

// 音频流信息
struct AudioStream : public StreamBaseInfo {
    int sampleRate{0};              // 采样率 Hz
    int channels{0};                // 声道数
    QString channelLayout;          // 声道布局
    QString sampleFormat;           // 采样格式
    int bitDepth{0};                // 位深 ?
};

// 视频流信息
struct VideoStream : public StreamBaseInfo {
    int width{0};
    int height{0};
    double fps{0.0};                // 帧率
    QString pixelFormat;            // 像素格式 (e.g. yuv420p)
    int bitDepth{0};                // 位深

    QString sar;                    // 像素宽高比 (Sample Aspect Ratio)
    QString dar;                    // 显示宽高比 (Display Aspect Ratio)

    // 色彩空间特性与 HDR
    QString colorRange;             // Limited / Full
    QString colorSpace;             // 色彩空间 (e.g. BT.709, BT.2020)
    QString colorTransfer;          // 传输特性 (PQ, HLG 等)
    QString colorPrimaries;         // 色彩原色
    QString hdrFormat;              // 综合判断出的 HDR 格式 (Dolby Vision / HDR10+ 等)

    int rotation{0};           // 视频旋转角度
    bool isAttachedPic{false};      // 是否为附属图片 (如封面)
};

// 字幕流信息
struct SubtitleStream : public StreamBaseInfo {
    bool isImageBased{false};       // 是否为图片字幕 (如蓝光 PGS)
    int width{0};                   // 宽度 (图片字幕适用)
    int height{0};                  // 高度 (图片字幕适用)
};

// 完整的媒体详情
struct MediaInfo {
    QString url;
    QString format;                 // 封装格式名称
    QString formatLongName;         // 封装格式全称
    int64_t fileSize{0};            // 文件大小 (Bytes)
    double duration{0.0};           // 容器时长 (秒)
    int64_t bitRate{0};             // 容器总码率 (bps)

    QMap<QString, QString> metadata; // 容器元数据

    QList<ChapterInfo> chapters;
    QList<VideoStream> videoStreams;
    QList<AudioStream> audioStreams;
    QList<SubtitleStream> subtitleStreams;
};

// 外挂字幕
struct ExternalSubtitle
{
    double start;
    double end;
    QString text;
};

}

Q_DECLARE_METATYPE(XC::BaseInfo)

#endif // MEDIAINFO_H
