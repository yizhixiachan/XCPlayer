#ifndef XCFL_H
#define XCFL_H

#include "utility/MediaInfo.h"

class XCFL
{
public:
    XCFL() = delete;
    ~XCFL() = delete;
    XCFL(const XCFL&) = delete;
    XCFL& operator=(const XCFL&) = delete;

    // 格式化歌词字符串
    static QMap<double, QPair<QString, QString>> FormatLyrics(const QString& lyrics);
    // 解析外挂字幕
    static QList<XC::ExternalSubtitle> ParseSubtitle(const QString& localPath);
    // 解码文本文件 (UTF-8 / GBK / UTF-16)
    static QString DecodeText(const std::string& str);


    struct FastColor {
        int r, g, b;
        bool operator==(const FastColor& o) const { return r == o.r && g == o.g && b == o.b; }
    };
    static QList<QColor> ExtractDominantColors(const QPixmap& src, int k, int maxIterations = 10);
private:
    static double ParseTime(const QString& timeStr);
    static QList<XC::ExternalSubtitle> ParseASS(const QString& content);
    static QList<XC::ExternalSubtitle> ParseSRTorVTT(const QString& content);

    static int ColorDistance(const FastColor& c1, const FastColor& c2);
};

#endif // XCFL_H
