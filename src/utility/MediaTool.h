#ifndef MEDIATOOL_H
#define MEDIATOOL_H

#include "MediaInfo.h"
#include <QImage>
extern "C"
{
#include "libavutil/pixfmt.h"
}

class AVDictionary;
class AVFormatContext;

class MediaTool
{
public:
    MediaTool() = delete;
    ~MediaTool() = delete;
    MediaTool(const MediaTool&) = delete;
    MediaTool& operator=(const MediaTool&) = delete;

    static XC::BaseInfo ExtractBaseInfo(AVFormatContext* ic);
    static XC::BaseInfo ExtractBaseInfo(const QString& url, std::atomic<bool>& stopFlag, AVDictionary* opts = nullptr);
    static XC::MediaInfo ExtractMediaInfo(AVFormatContext* ic);
    static XC::MediaInfo ExtractMediaInfo(const QString& url, std::atomic<bool>& stopFlag, AVDictionary* opts = nullptr);

    static QImage ExtractEmbeddedCover(const QString& url, std::atomic<bool>& stopFlag);
    static QImage ExtractVideoFrame(const QString& url, std::atomic<bool>& stopFlag);

    static bool ModifyMetadata(const QString& inputUrl, const QString& outputUrl, const QMap<QString, QString>& metadata, QString& error, std::atomic<bool>& stopFlag);
    static bool ReplaceCover(const QString &inputUrl, const QString &outputUrl, const QString &imageUrl, QString& error, std::atomic<bool> &stopFlag);
private:
    static QString CheckHDRFormat(enum AVColorTransferCharacteristic trc, bool isDOVI, bool isHDR10PLUS, bool isHDR10);
    static void ExtractMetadata(AVDictionary* dict, QMap<QString, QString>& map);
    static QString FormatLanguage(const QString& langCode, const QString& title);
};

#endif // MEDIATOOL_H
