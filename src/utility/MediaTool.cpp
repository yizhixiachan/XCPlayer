#include "MediaTool.h"
#include <filesystem>
#include <regex>
#include <QHash>
#include <QFileInfo>
extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/pixdesc.h"
#include "libavcodec/codec_desc.h"
#include "libavutil/display.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

static int Callback(void* opaque)
{
    auto* bStopReq = static_cast<std::atomic<bool>*>(opaque);
    if(bStopReq && bStopReq->load(std::memory_order_acquire)) return 1;
    return 0;
}

XC::BaseInfo MediaTool::ExtractBaseInfo(AVFormatContext *ic)
{
    XC::BaseInfo info{};
    if(!ic) return info;

    info.url = ic->url;

    // 元数据提取
    auto getTag = [&](const char* keyName) -> QString {
        QString target = QString::fromUtf8(keyName);
        AVDictionaryEntry* tag = nullptr;
        while((tag = av_dict_get(ic->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            if(QString::fromUtf8(tag->key).compare(target, Qt::CaseInsensitive) == 0) return QString::fromUtf8(tag->value);
        }
        for(unsigned int i = 0; i < ic->nb_streams; i++) {
            if(ic->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                tag = nullptr;
                while((tag = av_dict_get(ic->streams[i]->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
                    if(QString::fromUtf8(tag->key).compare(target, Qt::CaseInsensitive) == 0) return QString::fromUtf8(tag->value);
                }
            }
        }
        return "";
    };

    info.title = getTag("title");
    QString rawArtist = getTag("artist");
    if(!rawArtist.isEmpty()) {
        std::string artistStdStr = rawArtist.toStdString();
        static const std::regex re(R"(\s*(?:;|、)\s*)");
        artistStdStr = std::regex_replace(artistStdStr, re, "/");
        info.artist = QString::fromStdString(artistStdStr);
    }
    info.album = getTag("album");

    if(ic->duration != AV_NOPTS_VALUE) {
        info.duration = ic->duration * av_q2d(AV_TIME_BASE_Q);
    }

    bool hasVideo = false;

    // 寻找视频流
    int videoIndex = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if(videoIndex >= 0) {
        AVStream* st = ic->streams[videoIndex];
        AVCodecParameters* codecpar = st->codecpar;

        // 排除封面图片流
        if(!(st->disposition & AV_DISPOSITION_ATTACHED_PIC)){
            info.isVideo = true;

            // 帧率
            if(st->avg_frame_rate.den && st->avg_frame_rate.num) {
                info.num = st->avg_frame_rate.num;
                info.den = st->avg_frame_rate.den;
            } else if(st->r_frame_rate.den && st->r_frame_rate.num) {
                info.num = st->r_frame_rate.num;
                info.den = st->r_frame_rate.den;
            } else if(codecpar->framerate.den && codecpar->framerate.num){
                info.num = codecpar->framerate.num;
                info.den = codecpar->framerate.den;
            }

            // 分辨率
            info.width = codecpar->width;
            info.height = codecpar->height;

            // HDR
            bool isDOVI = false;
            bool isHDR10PLUS = false;
            bool isHDR10 = false;

            if(codecpar->nb_coded_side_data > 0) {
                // 杜比视界
                if(av_packet_side_data_get(codecpar->coded_side_data, codecpar->nb_coded_side_data, AV_PKT_DATA_DOVI_CONF)) {
                    isDOVI = true;
                }
                // HDR10+
                if(av_packet_side_data_get(codecpar->coded_side_data, codecpar->nb_coded_side_data, AV_PKT_DATA_DYNAMIC_HDR10_PLUS)) {
                    isHDR10PLUS = true;
                }
                // HDR10
                if(av_packet_side_data_get(codecpar->coded_side_data, codecpar->nb_coded_side_data, AV_PKT_DATA_MASTERING_DISPLAY_METADATA) ||
                    av_packet_side_data_get(codecpar->coded_side_data, codecpar->nb_coded_side_data, AV_PKT_DATA_CONTENT_LIGHT_LEVEL)) {
                    isHDR10 = true;
                }
            }

            info.hdrFormat = CheckHDRFormat(codecpar->color_trc, isDOVI, isHDR10PLUS, isHDR10);
        }

    }

    if(!info.isVideo || info.title.isEmpty()) {
        QFileInfo fileInfo(info.url);
        QString baseName = fileInfo.completeBaseName();

        if(!info.isVideo) {
            if(info.artist.isEmpty()) {
                QStringList parts = baseName.split(" - ");
                if(parts.size() == 2) {
                    if(info.title.isEmpty()) info.title = parts[0].trimmed();
                    if(info.artist.isEmpty()) info.artist = parts[1].trimmed();
                }
            }

            if(info.artist.isEmpty()) info.artist = "未知艺术家";
            if(info.album.isEmpty()) info.album = "未知专辑";
        }

        if(info.title.isEmpty()) {
            info.title = baseName;
        }
    }

    return info;
}

XC::BaseInfo MediaTool::ExtractBaseInfo(const QString &url, std::atomic<bool>& stopFlag, AVDictionary* opts)
{
    XC::BaseInfo info{};

    AVFormatContext* ic = avformat_alloc_context();
    ic->interrupt_callback.callback = Callback;
    ic->interrupt_callback.opaque = &stopFlag;

    int ret = -1;

    ret = avformat_open_input(&ic, url.toUtf8().constData(), nullptr, &opts);
    if(opts) av_dict_free(&opts);
    if(ret < 0) {
        return info;
    }

    ret = avformat_find_stream_info(ic, nullptr);
    if(ret < 0) {
        avformat_close_input(&ic);
        return info;
    }

    info = ExtractBaseInfo(ic);

    avformat_close_input(&ic);
    return info;
}

XC::MediaInfo MediaTool::ExtractMediaInfo(AVFormatContext *ic)
{

    XC::MediaInfo info{};
    if(!ic) return info;


    info.url = ic->url;
    // --- 容器信息 ---
    // 容器格式
    if(ic->iformat) {
        info.format = QString::fromUtf8(ic->iformat->name);
        info.formatLongName = QString::fromUtf8(ic->iformat->long_name);
    }
    // 容器时长
    if(ic->duration != AV_NOPTS_VALUE) {
        info.duration = ic->duration * av_q2d(AV_TIME_BASE_Q);
    }
    // 容器码率
    info.bitRate = ic->bit_rate;
    // 容器大小
    if(ic->pb) {
        info.fileSize = avio_size(ic->pb);
    }
    // 容器元数据
    ExtractMetadata(ic->metadata, info.metadata);

    // --- 章节信息 ---
    for(unsigned int i = 0; i < ic->nb_chapters; ++i) {
        AVChapter* ch = ic->chapters[i];
        XC::ChapterInfo chInfo;

        // 章节起止时间
        chInfo.startTime = ch->start * av_q2d(ch->time_base);
        chInfo.endTime = ch->end * av_q2d(ch->time_base);

        // 章节标题
        AVDictionaryEntry* tag = av_dict_get(ch->metadata, "title", nullptr, 0);
        if(tag && tag->value && *tag->value) {
            chInfo.title = QString::fromUtf8(tag->value);
        } else {
            chInfo.title = QString("Chapter %1").arg(i + 1);
        }
        info.chapters.append(chInfo);
    }

    // --- 流信息 ---
    for(unsigned int i = 0; i < ic->nb_streams; ++i) {
        AVStream* st = ic->streams[i];
        AVCodecParameters* codecpar = st->codecpar;

        XC::StreamBaseInfo baseInfo;
        // 流索引
        baseInfo.index = st->index;
        // 流元数据
        ExtractMetadata(st->metadata, baseInfo.metadata);

        // 语言
        QString langCode = baseInfo.metadata.value("language", "");
        QString title = baseInfo.metadata.value("title", "");
        baseInfo.language = FormatLanguage(langCode, title);

        // 流时长
        if(st->duration > 0 && st->duration != AV_NOPTS_VALUE) {
            baseInfo.duration = st->duration * av_q2d(st->time_base);

        } else {
            QString duration = baseInfo.metadata.value("DURATION");
            if(!duration.isEmpty()) {
                // 解析 HH:MM:SS.mmm 格式
                QStringList parts = duration.split(':');
                if(parts.size() == 3) {
                    double h = parts[0].toDouble();
                    double m = parts[1].toDouble();
                    double s = parts[2].toDouble();
                    baseInfo.duration = (h * 3600.0) + (m * 60.0) + s;
                }

            } else if(ic->duration > 0 && ic->duration != AV_NOPTS_VALUE) {
                baseInfo.duration = ic->duration * av_q2d(AV_TIME_BASE_Q);
            }
        }

        // 流码率
        if(codecpar->bit_rate > 0) {
            baseInfo.bitRate = codecpar->bit_rate;
        } else {
            QString bps = baseInfo.metadata.value("BPS");
            if(!bps.isEmpty()) {
                baseInfo.bitRate = bps.toLongLong();

            } else if(baseInfo.metadata.contains("NUMBER_OF_BYTES") && baseInfo.duration > 0.0) {
                int64_t totalBytes = baseInfo.metadata.value("NUMBER_OF_BYTES").toLongLong();
                baseInfo.bitRate = (totalBytes * 8) / baseInfo.duration;

            } else if(ic->nb_streams == 1 && ic->bit_rate > 0) {
                baseInfo.bitRate = ic->bit_rate;
            }
        }

        // 编解码器信息
        const AVCodecDescriptor* desc = avcodec_descriptor_get(codecpar->codec_id);
        if(desc) {
            baseInfo.codec = QString::fromUtf8(desc->name);
            if(desc->long_name) {
                baseInfo.codecLongName = QString::fromUtf8(desc->long_name);
            }

            // 编码档次（Profile）
            const char* profName = avcodec_profile_name(codecpar->codec_id, codecpar->profile);
            if(profName) {
                baseInfo.profile = QString::fromUtf8(profName);
            }
        }

        // 流特性标志
        if(st->disposition & AV_DISPOSITION_DEFAULT)           baseInfo.flags << "默认";
        if(st->disposition & AV_DISPOSITION_DUB)               baseInfo.flags << "其他语言";
        if(st->disposition & AV_DISPOSITION_ORIGINAL)          baseInfo.flags << "原始语言";
        if(st->disposition & AV_DISPOSITION_LYRICS)            baseInfo.flags << "歌词";
        if(st->disposition & AV_DISPOSITION_FORCED)            baseInfo.flags << "强制";
        if(st->disposition & AV_DISPOSITION_ATTACHED_PIC)      baseInfo.flags << "附加图片";

        // 流附加数据
        for(int sd_idx = 0; sd_idx < codecpar->nb_coded_side_data; ++sd_idx) {
            QString typeName = QString::fromUtf8(av_packet_side_data_name(codecpar->coded_side_data[sd_idx].type));
            baseInfo.sideDataList.append(typeName);
        }

        // --- 不同类型流的具体信息 ---
        if(codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            XC::AudioStream aInfo;
            // 拷贝流基础信息
            static_cast<XC::StreamBaseInfo&>(aInfo) = baseInfo;

            // 采样率
            aInfo.sampleRate = codecpar->sample_rate;
            // 声道数
            aInfo.channels = codecpar->ch_layout.nb_channels;
            // 声道布局
            char chLayoutName[128] = {0};
            av_channel_layout_describe(&codecpar->ch_layout, chLayoutName, sizeof(chLayoutName));
            aInfo.channelLayout = QString::fromUtf8(chLayoutName);

            // 采样格式
            const char* fmtName = av_get_sample_fmt_name((AVSampleFormat)codecpar->format);
            if(fmtName) aInfo.sampleFormat = QString::fromUtf8(fmtName);

            // 位深
            aInfo.bitDepth = codecpar->bits_per_raw_sample;

            info.audioStreams.append(aInfo);

        } else if(codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            XC::VideoStream vInfo;

            // 拷贝流基础信息
            static_cast<XC::StreamBaseInfo&>(vInfo) = baseInfo;

            // 打上封面标记
            if(st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                vInfo.isAttachedPic = true;
            }

            // 分辨率
            vInfo.width = codecpar->width;
            vInfo.height = codecpar->height;

            // 帧率
            if(st->avg_frame_rate.den && st->avg_frame_rate.num) {
                vInfo.fps = av_q2d(st->avg_frame_rate);
            } else if(st->r_frame_rate.den && st->r_frame_rate.num) {
                vInfo.fps = av_q2d(st->r_frame_rate);
            } else if(codecpar->framerate.den && codecpar->framerate.num){
                vInfo.fps = av_q2d(codecpar->framerate);
            }


            // 像素格式与位深
            const AVPixFmtDescriptor* pixDesc = av_pix_fmt_desc_get((AVPixelFormat)codecpar->format);
            if(pixDesc) {
                vInfo.pixelFormat = QString::fromUtf8(pixDesc->name);
                vInfo.bitDepth = pixDesc->comp[0].depth;
            }

            if (codecpar->sample_aspect_ratio.num > 0) {
                // 采样宽高比
                vInfo.sar = QString("%1:%2").arg(codecpar->sample_aspect_ratio.num).arg(codecpar->sample_aspect_ratio.den);
                // 显示宽高比
                AVRational dar;
                // 约分
                av_reduce(&dar.num, &dar.den, codecpar->width * codecpar->sample_aspect_ratio.num, codecpar->height * codecpar->sample_aspect_ratio.den, 1024*1024);
                vInfo.dar = QString("%1:%2").arg(dar.num).arg(dar.den);
            }

            // 色彩空间特性
            vInfo.colorRange = QString::fromUtf8(av_color_range_name(codecpar->color_range));
            vInfo.colorSpace = QString::fromUtf8(av_color_space_name(codecpar->color_space));
            vInfo.colorTransfer = QString::fromUtf8(av_color_transfer_name(codecpar->color_trc));
            vInfo.colorPrimaries = QString::fromUtf8(av_color_primaries_name(codecpar->color_primaries));

            // 旋转角度
            double rotation{0.0};
            const AVPacketSideData* sdMatrix = av_packet_side_data_get(codecpar->coded_side_data, codecpar->nb_coded_side_data, AV_PKT_DATA_DISPLAYMATRIX);
            if(sdMatrix) {
                rotation = av_display_rotation_get((int32_t*)sdMatrix->data);
            }
            if(!std::isnan(rotation)) {
                int angle = (int)std::round(rotation);
                if(angle != 0) {
                    vInfo.rotation = angle;
                }
            }

            // HDR 格式
            bool isDOVI = av_packet_side_data_get(codecpar->coded_side_data, codecpar->nb_coded_side_data, AV_PKT_DATA_DOVI_CONF) != nullptr;
            bool isHDR10Plus = av_packet_side_data_get(codecpar->coded_side_data, codecpar->nb_coded_side_data, AV_PKT_DATA_DYNAMIC_HDR10_PLUS) != nullptr;
            bool isHDR10 = av_packet_side_data_get(codecpar->coded_side_data, codecpar->nb_coded_side_data, AV_PKT_DATA_MASTERING_DISPLAY_METADATA) != nullptr;
            vInfo.hdrFormat = CheckHDRFormat(codecpar->color_trc, isDOVI, isHDR10Plus, isHDR10);

            info.videoStreams.append(vInfo);

        } else if(codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) {
            XC::SubtitleStream sInfo;

            // 拷贝流基础信息
            static_cast<XC::StreamBaseInfo&>(sInfo) = baseInfo;

            // 区分图形字幕和文本字幕
            if(codecpar->codec_id == AV_CODEC_ID_HDMV_PGS_SUBTITLE ||
                codecpar->codec_id == AV_CODEC_ID_DVD_SUBTITLE ||
                codecpar->codec_id == AV_CODEC_ID_XSUB) {
                sInfo.isImageBased = true;
                sInfo.width = codecpar->width;
                sInfo.height = codecpar->height;
            }

            info.subtitleStreams.append(sInfo);
        }
    }

    return info;
}

XC::MediaInfo MediaTool::ExtractMediaInfo(const QString &url, std::atomic<bool> &stopFlag, AVDictionary* opts)
{
    XC::MediaInfo info{};

    AVFormatContext* ic = avformat_alloc_context();
    ic->interrupt_callback.callback = Callback;
    ic->interrupt_callback.opaque = &stopFlag;

    int ret = -1;

    ret = avformat_open_input(&ic, url.toUtf8().constData(), nullptr, &opts);
    if(opts) av_dict_free(&opts);
    if(ret < 0) {
        return info;
    }

    ret = avformat_find_stream_info(ic, nullptr);
    if(ret < 0) {
        avformat_close_input(&ic);
        return info;
    }

    info = ExtractMediaInfo(ic);

    avformat_close_input(&ic);
    return info;
}

QImage MediaTool::ExtractEmbeddedCover(const QString &url, std::atomic<bool> &stopFlag)
{
    QImage cover;

    AVFormatContext* ic = avformat_alloc_context();
    ic->interrupt_callback.callback = Callback;
    ic->interrupt_callback.opaque = &stopFlag;

    if(avformat_open_input(&ic, url.toUtf8().constData(), nullptr, nullptr) < 0) {
        return cover;
    }

    if(avformat_find_stream_info(ic, nullptr) >= 0) {
        for(unsigned int i = 0; i < ic->nb_streams; ++i) {
            AVStream* st = ic->streams[i];
            if(st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                if(st->attached_pic.data && st->attached_pic.size > 0) {
                    cover.loadFromData(st->attached_pic.data, st->attached_pic.size);
                    break;
                }
            }
        }
    }

    avformat_close_input(&ic);
    return cover;
}

QImage MediaTool::ExtractVideoFrame(const QString &url, std::atomic<bool> &stopFlag)
{
    QImage videoFrame;
    AVFormatContext* ic = nullptr;
    AVCodecContext* codecCtx = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* pkt = nullptr;
    SwsContext* swsCtx = nullptr;

    const AVCodec* codec = nullptr;
    int videoStreamIdx = -1;
    AVStream* stream = nullptr;
    bool gotFrame = false;
    int maxAttempts = 1200;

    ic = avformat_alloc_context();
    ic->interrupt_callback.callback = Callback;
    ic->interrupt_callback.opaque = &stopFlag;

    if(avformat_open_input(&ic, url.toUtf8().constData(), nullptr, nullptr) < 0) goto cleanup;
    if(avformat_find_stream_info(ic, nullptr) < 0) goto cleanup;

    videoStreamIdx = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if(videoStreamIdx < 0) goto cleanup;

    stream = ic->streams[videoStreamIdx];

    // 处理附加图片流
    if(stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
        int realVideoIdx = -1;
        for(unsigned int i = 0; i < ic->nb_streams; ++i) {
            if(ic->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
                !(ic->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
                realVideoIdx = i;
                break;
            }
        }
        if(realVideoIdx != -1) {
            videoStreamIdx = realVideoIdx;
            stream = ic->streams[videoStreamIdx];
        } else {
            goto cleanup;
        }
    }

    codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if(!codec) goto cleanup;

    codecCtx = avcodec_alloc_context3(codec);
    if(!codecCtx) goto cleanup;

    if(avcodec_parameters_to_context(codecCtx, stream->codecpar) < 0) goto cleanup;
    if(avcodec_open2(codecCtx, codec, nullptr) < 0) goto cleanup;

    // 跳转到 3% 位置
    if(stream->duration > 0 && stream->duration != AV_NOPTS_VALUE) {
        int64_t targetTime = (int64_t)(stream->duration * 0.03);
        av_seek_frame(ic, videoStreamIdx, targetTime, AVSEEK_FLAG_BACKWARD);
    } else if(ic->duration > 0 && ic->duration != AV_NOPTS_VALUE) {
        int64_t targetTime = (int64_t)(ic->duration * 0.03);
        av_seek_frame(ic, -1, targetTime, AVSEEK_FLAG_BACKWARD);
    }

    avcodec_flush_buffers(codecCtx);


    frame = av_frame_alloc();
    pkt = av_packet_alloc();
    if(!frame || !pkt) goto cleanup;

    static int max_ = 0;
    // 循环读取和解码，直到找到一个真正的 I 帧
    while(!gotFrame && maxAttempts-- > 0 && av_read_frame(ic, pkt) >= 0) {
        if(pkt->stream_index == videoStreamIdx) {
            if(avcodec_send_packet(codecCtx, pkt) == 0) {
                while(avcodec_receive_frame(codecCtx, frame) == 0) {
                    // 检查该帧是否为 I 帧 (关键帧)
                    // TS 文件在 seek 后如果拿到 P/B 帧，这里会过滤掉，直到遇到 I 帧
                    // MP4 文件 seek 后通常直接就是 I 帧，直接通过
                    if (frame->pict_type == AV_PICTURE_TYPE_I) {
                        if (frame->width > 0 && frame->height > 0 && frame->format != AV_PIX_FMT_NONE) {
                            gotFrame = true;
                            break;
                        }
                    }
                }
            }
        }
        av_packet_unref(pkt);
    }

    // 如果尝试了1200次还没找到 I 帧（极其罕见的损坏文件），
    // 只要能解码出任何有效的帧，我们就勉强接受，防止完全没图
    if(!gotFrame && frame->width > 0 && frame->format != AV_PIX_FMT_NONE) {
        gotFrame = true;
    }

    if(!gotFrame || frame->format == AV_PIX_FMT_NONE) goto cleanup;

    swsCtx = sws_getContext(frame->width, frame->height, (AVPixelFormat)frame->format,
                            frame->width, frame->height, AV_PIX_FMT_RGB24,
                            SWS_BICUBIC, nullptr, nullptr, nullptr);
    if(!swsCtx) goto cleanup;

    {
        uint8_t* dst_data[4] = { nullptr };
        int dst_linesize[4] = { 0 };
        int ret = av_image_alloc(dst_data, dst_linesize, frame->width, frame->height, AV_PIX_FMT_RGB24, 32);
        if (ret < 0) goto cleanup;

        sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height, dst_data, dst_linesize);
        videoFrame = QImage(dst_data[0], frame->width, frame->height, dst_linesize[0], QImage::Format_RGB888).copy();
        av_freep(&dst_data[0]);

        // 旋转处理
        double rotation{0.0};
        const AVPacketSideData* sdMatrix = av_packet_side_data_get(stream->codecpar->coded_side_data,
                                                                   stream->codecpar->nb_coded_side_data,
                                                                   AV_PKT_DATA_DISPLAYMATRIX);
        if(sdMatrix) {
            rotation = av_display_rotation_get((int32_t*)sdMatrix->data);
        }
        if(!isnan(rotation)) {
            int angle = (int)std::round(rotation);
            if(angle != 0) {
                QTransform transform;
                transform.rotate(-angle);
                videoFrame = videoFrame.transformed(transform, Qt::SmoothTransformation);
            }
        }
    }

cleanup:
    if (swsCtx) sws_freeContext(swsCtx);
    if (pkt) av_packet_free(&pkt);
    if (frame) av_frame_free(&frame);
    if (codecCtx) avcodec_free_context(&codecCtx);
    if (ic) avformat_close_input(&ic);

    return videoFrame;
}

bool MediaTool::ModifyMetadata(const QString &inputUrl, const QString &outputUrl, const QMap<QString, QString> &metadata, QString& error, std::atomic<bool> &stopFlag)
{
    auto getError = [](int ret) {
        char errbuf[1024] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        return QString::fromUtf8(errbuf);
    };

    std::filesystem::path inputPath;
#ifdef Q_OS_WIN
    inputPath = inputUrl.toStdWString();
#else
    inputPath = inputUrl.toStdString();
#endif

    std::error_code ec;
    if(!std::filesystem::exists(inputPath, ec) || !std::filesystem::is_regular_file(inputPath, ec)) {
        error = "无效文件";
        return false;
    }

    // 1. 确定工作模式与路径
    bool inPlace = outputUrl.trimmed().isEmpty() || outputUrl == inputUrl;

    QFileInfo fi(inputUrl);
    QString finalOutUrl = outputUrl;
    if(inPlace) {
        QString ext = fi.suffix().isEmpty() ? ".tmp" : ("." + fi.suffix());
        finalOutUrl = fi.absolutePath() + "/" + fi.completeBaseName() + "_tmp" + ext;
    }

    AVFormatContext* ic = nullptr;
    AVFormatContext* oc = nullptr;
    AVPacket* pkt = nullptr;
    bool success = true;
    int ret = 0;

    // 用于记录输入流索引到输出流索引的映射。-1表示该流被丢弃
    QList<int> stream_mapping;
    int out_stream_index = 0;

    // [新增]: 智能应用元数据 Lambda，保留原有大小写键名 支持置空删除
    auto applyFuzzyMeta = [](AVDictionary** dict, const QMap<QString, QString>& newMeta) {
        for(auto it = newMeta.constBegin(); it != newMeta.constEnd(); ++it) {
            QString targetKey = it.key();
            QString val = it.value().trimmed(); // 去除首尾空格

            AVDictionaryEntry* tag = nullptr;
            bool found = false;

            // 模糊查找原本是否存在这个字段，保留大小写
            while((tag = av_dict_get(*dict, "", tag, AV_DICT_IGNORE_SUFFIX))) {
                if(QString::fromUtf8(tag->key).compare(targetKey, Qt::CaseInsensitive) == 0) {
                    targetKey = QString::fromUtf8(tag->key);
                    found = true;
                    break;
                }
            }
            // 针对歌词特判 (可能是 lyrics-xxx 等变体)
            if(!found && targetKey.compare("lyrics", Qt::CaseInsensitive) == 0) {
                tag = nullptr;
                while((tag = av_dict_get(*dict, "", tag, AV_DICT_IGNORE_SUFFIX))) {
                    if(QString::fromUtf8(tag->key).startsWith("lyric", Qt::CaseInsensitive)) {
                        targetKey = QString::fromUtf8(tag->key);
                        found = true;
                        break;
                    }
                }
            }

            // 【核心修改】：如果输入值为空，直接传入 nullptr 将其从文件中物理删除
            if (val.isEmpty()) {
                if (found) {
                    av_dict_set(dict, targetKey.toUtf8().constData(), nullptr, 0);
                }
            } else {
                av_dict_set(dict, targetKey.toUtf8().constData(), val.toUtf8().constData(), 0);
            }
        }
    };

    ic = avformat_alloc_context();
    ic->interrupt_callback.callback = Callback;
    ic->interrupt_callback.opaque = &stopFlag;

    ret = avformat_open_input(&ic, inputUrl.toUtf8().constData(), nullptr, nullptr);
    if(ret < 0) {
        error = QString("打开媒体文件失败：%1").arg(getError(ret));
        return false;
    }
    stream_mapping.fill(-1, ic->nb_streams);

    ret = avformat_find_stream_info(ic, nullptr);
    if (ret < 0) {
        avformat_close_input(&ic);
        error = QString("读取流信息失败：%1").arg(getError(ret));
        return false;
    }

    ret = avformat_alloc_output_context2(&oc, nullptr, nullptr, finalOutUrl.toUtf8().constData());
    if (!oc || ret < 0) {
        success = false;
        error = QString("创建输出上下文失败：%1").arg(getError(ret));
        goto cleanup_ffmpeg;
    }

    {
        QString outFormatName  = QString::fromUtf8(oc->oformat->name).toLower();
        bool isOggFamily = outFormatName.contains("ogg") || outFormatName.contains("oga") || outFormatName.contains("opus");
        QString preservedBase64Cover;

        // 4. 复制流、编解码参数
        for (unsigned int i = 0; i < ic->nb_streams; i++) {
            AVStream *in_stream = ic->streams[i];

            if (in_stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
                in_stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
                continue;
            }

            // 【关键修复】：OGG / OPUS 封装器不支持写入视频流（封面）
            // 如果将封面的视频流强传给 OGG Muxer，会导致 avformat_write_header 返回 -22 (Invalid argument)。
            // 因此对于 OGG 家族，我们拦截图片流，将其重新打包为 Base64，稍后通过 metadata 安全写入。
            if (isOggFamily && in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
                (in_stream->disposition & AV_DISPOSITION_ATTACHED_PIC)) {

                if(in_stream->attached_pic.size > 0 && in_stream->attached_pic.data) {
                    std::vector<uint8_t> imgData(in_stream->attached_pic.data,
                                                 in_stream->attached_pic.data + in_stream->attached_pic.size);
                    std::string mime;
                    switch (in_stream->codecpar->codec_id) {
                    case AV_CODEC_ID_PNG:  mime = "image/png"; break;
                    case AV_CODEC_ID_BMP:  mime = "image/bmp"; break;
                    case AV_CODEC_ID_GIF:  mime = "image/gif"; break;
                    case AV_CODEC_ID_WEBP: mime = "image/webp"; break;
                    case AV_CODEC_ID_TIFF: mime = "image/tiff"; break;
                    default: mime = "image/jpeg"; break;
                    }

                    std::vector<uint8_t> flacBlock;
                    // 辅助函数：以大端序写入32位整数
                    auto writeBigEndian32 = [&](uint32_t value) {
                        flacBlock.push_back((value >> 24) & 0xFF);
                        flacBlock.push_back((value >> 16) & 0xFF);
                        flacBlock.push_back((value >> 8) & 0xFF);
                        flacBlock.push_back(value & 0xFF);
                    };

                    writeBigEndian32(3);                                          // 封面类型 3 (Front Cover)
                    writeBigEndian32(mime.length());                              // MIME 字符串长度
                    flacBlock.insert(flacBlock.end(), mime.begin(), mime.end());  // MIME 字符串内容
                    writeBigEndian32(0);                                          // 描述字符串长度 (0)
                    writeBigEndian32(in_stream->codecpar->width);                 // 图像宽
                    writeBigEndian32(in_stream->codecpar->height);                // 图像高
                    writeBigEndian32(24);                                         // 颜色深度 (一般设为24)
                    writeBigEndian32(0);                                          // 索引颜色数 (0)
                    writeBigEndian32(imgData.size());                             // 图像数据长度
                    flacBlock.insert(flacBlock.end(), imgData.begin(), imgData.end()); // 图像数据本体

                    // Base64 编码
                    static const char base64Chars[] =
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

                    std::string base64Cover;
                    base64Cover.reserve(((flacBlock.size() + 2) / 3) * 4);

                    for (size_t j = 0; j < flacBlock.size(); j += 3) {
                        uint32_t triple = (uint32_t)flacBlock[j] << 16;
                        if (j + 1 < flacBlock.size()) triple |= (uint32_t)flacBlock[j + 1] << 8;
                        if (j + 2 < flacBlock.size()) triple |= (uint32_t)flacBlock[j + 2];

                        base64Cover += base64Chars[(triple >> 18) & 0x3F];
                        base64Cover += base64Chars[(triple >> 12) & 0x3F];
                        base64Cover += (j + 1 < flacBlock.size()) ? base64Chars[(triple >> 6) & 0x3F] : '=';
                        base64Cover += (j + 2 < flacBlock.size()) ? base64Chars[triple & 0x3F] : '=';
                    }

                    preservedBase64Cover = QString::fromStdString(base64Cover);
                }
                continue; // 丢弃该流，不再为其创建输出流
            }

            AVStream *out_stream = avformat_new_stream(oc, nullptr);
            if(!out_stream) {
                success = false;
                error = "创建输出流失败";
                goto cleanup_ffmpeg;
            }

            ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
            if(ret < 0) {
                success = false;
                error = QString("复制流的编解码参数失败：%1").arg(getError(ret));
                goto cleanup_ffmpeg;
            }

            out_stream->time_base = in_stream->time_base;
            out_stream->disposition = in_stream->disposition;

            if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                out_stream->codecpar->codec_tag = 0;
            } else {
                out_stream->codecpar->codec_tag = in_stream->codecpar->codec_tag;
            }

            av_dict_copy(&out_stream->metadata, in_stream->metadata, 0);

            stream_mapping[i] = out_stream_index++;
        }

        // 5. 复制音频文件的章节信息
        if (ic->nb_chapters > 0) {
            oc->chapters = (AVChapter**)av_calloc(ic->nb_chapters, sizeof(AVChapter*));
            if(oc->chapters) {
                oc->nb_chapters = ic->nb_chapters;
                for(unsigned int i = 0; i < ic->nb_chapters; i++) {
                    AVChapter *in_ch = ic->chapters[i];
                    AVChapter *out_ch = (AVChapter*)av_mallocz(sizeof(AVChapter));
                    out_ch->id = in_ch->id;
                    out_ch->time_base = in_ch->time_base;
                    out_ch->start = in_ch->start;
                    out_ch->end = in_ch->end;
                    av_dict_copy(&out_ch->metadata, in_ch->metadata, 0);
                    oc->chapters[i] = out_ch;
                }
            }
        }

        // 6. 处理全局和流级别元数据
        av_dict_copy(&oc->metadata, ic->metadata, 0);
        applyFuzzyMeta(&oc->metadata, metadata);

        // 如果拦截到了 OGG 封面，安全地塞入全局 metadata
        if (isOggFamily && !preservedBase64Cover.isEmpty()) {
            av_dict_set(&oc->metadata, "METADATA_BLOCK_PICTURE", preservedBase64Cover.toUtf8().constData(), 0);
        }

        for (unsigned int i = 0; i < oc->nb_streams; i++) {
            if (oc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                applyFuzzyMeta(&oc->streams[i]->metadata, metadata);
                // 规范要求 OGG 流 metadata 也要写入一份
                if (isOggFamily && !preservedBase64Cover.isEmpty()) {
                    av_dict_set(&oc->streams[i]->metadata, "METADATA_BLOCK_PICTURE", preservedBase64Cover.toUtf8().constData(), 0);
                }
            }
        }
    }

    // 打开输出文件写入
    if(!(oc->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open2(&oc->pb, finalOutUrl.toUtf8().constData(), AVIO_FLAG_WRITE, nullptr, nullptr);
        if(ret < 0) {
            success = false;
            error = QString("打开输出文件失败：%1").arg(getError(ret));
            goto cleanup_ffmpeg;
        }
    }

    // 写入文件头
    ret = avformat_write_header(oc, nullptr);
    if (ret < 0) {
        success = false;
        error = QString("写入文件头失败：%1").arg(getError(ret));
        goto cleanup_ffmpeg;
    }

    // 8. 逐帧读取并写入新文件
    pkt = av_packet_alloc();
    while(true) {
        if (stopFlag.load(std::memory_order_acquire)) {
            success = false;
            error = "用户已中止操作";
            break;
        }

        ret = av_read_frame(ic, pkt);
        if(ret < 0) {
            if(ret == AVERROR_EOF) {
                break;
            }
            success = false;
            error = QString("读取数据帧失败：%1").arg(getError(ret));
            break;
        }

        int in_stream_index = pkt->stream_index;
        // 如果是已被丢弃的 OGG 封面流或章节流，映射值为 -1，安全略过
        if (in_stream_index < 0 || in_stream_index >= stream_mapping.size() || stream_mapping[in_stream_index] < 0) {
            av_packet_unref(pkt);
            continue;
        }

        pkt->stream_index = stream_mapping[in_stream_index];

        AVStream *in_stream = ic->streams[in_stream_index];
        AVStream *out_stream = oc->streams[pkt->stream_index];

        av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);
        pkt->pos = -1;

        ret = av_interleaved_write_frame(oc, pkt);
        if (ret < 0) {
            success = false;
            error = QString("写入数据帧失败：%1").arg(getError(ret));
            av_packet_unref(pkt);
            break;
        }
        av_packet_unref(pkt);
    }

    // 9. 写入文件尾
    if (success) {
        ret = av_write_trailer(oc);
        if (ret < 0) {
            success = false;
            error = QString("写入文件尾失败：%1").arg(getError(ret));
        }
    }

cleanup_ffmpeg:
    // ==== 彻底释放句柄 ====
    if (pkt) av_packet_free(&pkt);
    if (ic) avformat_close_input(&ic);
    if (oc && !(oc->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&oc->pb);
    }
    if (oc) avformat_free_context(oc);


    // 文件安全替换
    std::filesystem::path tmpPath;
#ifdef Q_OS_WIN
    tmpPath = finalOutUrl.toStdWString();
#else
    tmpPath = finalOutUrl.toStdString();
#endif

    if(inPlace) {
        if(success) {
            if(std::filesystem::file_size(tmpPath, ec) == 0 || ec) {
                success = false;
                error = "生成的临时文件无效";
            } else {
                AVFormatContext* checkCtx = nullptr;
                // 用 FFmpeg 校验刚生成的文件是否结构完整
                if(avformat_open_input(&checkCtx, finalOutUrl.toUtf8().constData(), nullptr, nullptr) == 0) {
                    if(avformat_find_stream_info(checkCtx, nullptr) < 0) {
                        success = false;
                        error = "生成的临时文件无效";
                    }
                    avformat_close_input(&checkCtx);
                } else {
                    success = false;
                    error = "生成的临时文件无效";
                }
            }
        }

        if (success) {
            std::filesystem::path backupPath;
            std::filesystem::path inPath;
#ifdef Q_OS_WIN
            backupPath = (inputUrl + ".bak").toStdWString();
            inPath = inputUrl.toStdWString();
#else
            backupPath = (inputUrl + ".bak").toStdString();
            inPath = inputUrl.toStdString();
#endif

            // 删除可能存在的上一次残留备份
            std::filesystem::remove(backupPath, ec);
            // 将原文件重命名为 xxx.bak
            std::filesystem::rename(inPath, backupPath, ec);
            if(!ec) {
                // 将临时文件重命名为原文件
                std::filesystem::rename(tmpPath, inPath, ec);
                if(!ec) {
                    // 成功替换
                    std::filesystem::remove(backupPath, ec);
                }
                // 重命名失败
                else {
                    // 替换失败，回滚
                    std::filesystem::rename(backupPath, inPath, ec);
                    std::filesystem::remove(tmpPath, ec);
                    success = false;
                    error = "文件替换失败，已自动回滚";
                }
            }
            // 重命名失败
            else {
                std::filesystem::remove(tmpPath, ec);
                success = false;
                error = "文件可能正在使用";
            }
        } else {
            std::filesystem::remove(tmpPath, ec);
        }
    } else {
        if (!success) {
            std::filesystem::remove(tmpPath, ec);
        }
    }
    return success;
}

bool MediaTool::ReplaceCover(const QString &inputUrl, const QString &outputUrl, const QString &imageUrl, QString& error, std::atomic<bool> &stopFlag)
{
    auto getError = [](int ret) {
        char errbuf[1024] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        return QString::fromUtf8(errbuf);
    };

    std::filesystem::path inputPath;
    std::filesystem::path imagePath;
#ifdef Q_OS_WIN
    inputPath = inputUrl.toStdWString();
    imagePath = imageUrl.toStdWString();
#else
    inputPath = inputUrl.toStdString();
    imagePath = imageUrl.toStdString();
#endif

    std::error_code ec;
    if(!std::filesystem::exists(inputPath, ec) || !std::filesystem::is_regular_file(inputPath, ec)) {
        error = "无效文件";
        return false;
    }

    if(!std::filesystem::exists(imagePath, ec) || !std::filesystem::is_regular_file(imagePath, ec)) {
        error = "无效图片文件";
        return false;
    }

    bool inPlace = outputUrl.trimmed().isEmpty() || outputUrl == inputUrl;

    QFileInfo fi(inputUrl);
    QString finalOutUrl = outputUrl;
    if (inPlace) {
        QString ext = fi.suffix().isEmpty() ? ".tmp" : ("." + fi.suffix());
        finalOutUrl = fi.absolutePath() + "/" + fi.completeBaseName() + "_tmp" + ext;
    }

    AVFormatContext *ic_audio = nullptr;
    AVFormatContext *ic_image = nullptr;
    AVFormatContext *oc = nullptr;
    AVPacket *pkt = nullptr;
    bool success = true;
    int image_stream_idx = -1;
    int ret = 0;
    QMap<int, int> streamMapping;

    pkt = av_packet_alloc();

    ret = avformat_open_input(&ic_audio, inputUrl.toUtf8().constData(), nullptr, nullptr);
    if(ret < 0) {
        success = false;
        error = QString("打开媒体文件失败：%1").arg(getError(ret));
        goto cleanup;
    }

    ret = avformat_find_stream_info(ic_audio, nullptr);
    if(ret < 0) {
        success = false;
        error = QString("读取媒体流信息失败：%1").arg(getError(ret));
        goto cleanup;
    }

    ret = avformat_open_input(&ic_image, imageUrl.toUtf8().constData(), nullptr, nullptr);
    if(ret < 0) {
        success = false;
        error = QString("打开图片文件失败：%1").arg(getError(ret));
        goto cleanup;
    }

    ret = avformat_find_stream_info(ic_image, nullptr);
    if(ret < 0) {
        success = false;
        error = QString("读取图片流信息失败：%1").arg(getError(ret));
        goto cleanup;
    }

    ret = avformat_alloc_output_context2(&oc, nullptr, nullptr, finalOutUrl.toUtf8().constData());
    if(!oc || ret < 0) {
        success = false;
        error = QString("创建输出上下文失败: %1").arg(getError(ret));
        goto cleanup;
    }

    // ================== 1. 构建输出音频流 ==================
    for (unsigned int i = 0; i < ic_audio->nb_streams; i++) {
        AVStream *in_stream = ic_audio->streams[i];

        // 【终极必杀】：对于音频文件，我们*只*保留音频流！
        // 抛弃所有的视频流(旧封面)、数据流(旧章节)、文本流(旧字幕)。
        // 强行把旧的私有数据流拷过去，100% 会导致 M4A write_header 冲突报错！
        if(in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            AVStream *out_stream = avformat_new_stream(oc, nullptr);
            if(!out_stream) {
                success = false;
                error = "创建音频输出流失败";
                goto cleanup;
            }

            ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
            if(ret < 0) {
                success = false;
                error = QString("复制流的编解码参数失败: %1").arg(getError(ret));
                goto cleanup;
            }

            out_stream->time_base = in_stream->time_base;
            out_stream->disposition = in_stream->disposition;

            // 无条件清空 tag，让封装器(如 ipod/mp4)自己分配最标准的标签
            out_stream->codecpar->codec_tag = 0;

            av_dict_copy(&out_stream->metadata, in_stream->metadata, 0);

            // 清理旧的 OGG Base64 封面残留
            av_dict_set(&out_stream->metadata, "METADATA_BLOCK_PICTURE", nullptr, 0);

            streamMapping[i] = out_stream->index;
        }
    }

    if (ic_audio->nb_chapters > 0) {
        oc->chapters = (AVChapter**)av_calloc(ic_audio->nb_chapters, sizeof(AVChapter*));
        if (oc->chapters) {
            oc->nb_chapters = ic_audio->nb_chapters;
            for (unsigned int i = 0; i < ic_audio->nb_chapters; i++) {
                oc->chapters[i] = (AVChapter*)av_mallocz(sizeof(AVChapter));
                oc->chapters[i]->id = ic_audio->chapters[i]->id;
                oc->chapters[i]->time_base = ic_audio->chapters[i]->time_base;
                oc->chapters[i]->start = ic_audio->chapters[i]->start;
                oc->chapters[i]->end = ic_audio->chapters[i]->end;
                av_dict_copy(&oc->chapters[i]->metadata, ic_audio->chapters[i]->metadata, 0);
            }
        }
    }

    av_dict_copy(&oc->metadata, ic_audio->metadata, 0);
    av_dict_set(&oc->metadata, "METADATA_BLOCK_PICTURE", nullptr, 0); // 清理旧残留

    // ================== 2. 提取并准备新图片 ==================
    {
        AVPacket *imgPkt = av_packet_alloc();
        AVStream *in_img_stream = nullptr;
        bool gotImage = false;

        while (av_read_frame(ic_image, imgPkt) >= 0) {
            if(ic_image->streams[imgPkt->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                in_img_stream = ic_image->streams[imgPkt->stream_index];
                gotImage = true;
                break;
            }
            av_packet_unref(imgPkt);
        }

        if(!gotImage) {
            success = false;
            error = "无效图像数据";
            av_packet_free(&imgPkt);
            goto cleanup;
        }

        bool useBase64Mode = false;
        QString outFormatName  = QString::fromUtf8(oc->oformat->name).toLower();
        useBase64Mode = outFormatName.contains("ogg") || outFormatName.contains("oga") || outFormatName.contains("opus");

        if (useBase64Mode) {
            // 【Base64 编码模式】：针对 OGG / OPUS
            std::vector<uint8_t> imgData((const uint8_t*)imgPkt->data,
                                         (const uint8_t*)imgPkt->data + imgPkt->size);

            std::string mime;
            switch (in_img_stream->codecpar->codec_id) {
            case AV_CODEC_ID_PNG:  mime = "image/png"; break;
            case AV_CODEC_ID_BMP:  mime = "image/bmp"; break;
            case AV_CODEC_ID_GIF:  mime = "image/gif"; break;
            case AV_CODEC_ID_WEBP: mime = "image/webp"; break;
            case AV_CODEC_ID_TIFF: mime = "image/tiff"; break;
            default: mime = "image/jpeg"; break;
            }

            // 组装标准的 FLAC Picture Block
            std::vector<uint8_t> flacBlock;

            // 辅助函数：以大端序写入32位整数
            auto writeBigEndian32 = [&](uint32_t value) {
                flacBlock.push_back((value >> 24) & 0xFF);
                flacBlock.push_back((value >> 16) & 0xFF);
                flacBlock.push_back((value >> 8) & 0xFF);
                flacBlock.push_back(value & 0xFF);
            };

            writeBigEndian32(3);                                          // 封面类型 3 (Front Cover)
            writeBigEndian32(mime.length());                              // MIME 长度
            flacBlock.insert(flacBlock.end(), mime.begin(), mime.end());  // MIME 字符串
            writeBigEndian32(0);                                          // 描述字符串长度
            writeBigEndian32(in_img_stream->codecpar->width);             // 宽
            writeBigEndian32(in_img_stream->codecpar->height);            // 高
            writeBigEndian32(24);                                         // 色深
            writeBigEndian32(0);                                          // 索引色数
            writeBigEndian32(imgData.size());                             // 数据长度
            flacBlock.insert(flacBlock.end(), imgData.begin(), imgData.end()); // 图像数据

            // Base64 编码
            static const char base64Chars[] =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

            std::string base64Cover;
            base64Cover.reserve(((flacBlock.size() + 2) / 3) * 4);

            for (size_t i = 0; i < flacBlock.size(); i += 3) {
                uint32_t triple = (uint32_t)flacBlock[i] << 16;
                if (i + 1 < flacBlock.size()) triple |= (uint32_t)flacBlock[i + 1] << 8;
                if (i + 2 < flacBlock.size()) triple |= (uint32_t)flacBlock[i + 2];

                base64Cover += base64Chars[(triple >> 18) & 0x3F];
                base64Cover += base64Chars[(triple >> 12) & 0x3F];
                base64Cover += (i + 1 < flacBlock.size()) ? base64Chars[(triple >> 6) & 0x3F] : '=';
                base64Cover += (i + 2 < flacBlock.size()) ? base64Chars[triple & 0x3F] : '=';
            }

            av_dict_set(&oc->metadata, "METADATA_BLOCK_PICTURE", base64Cover.c_str(), 0);

            // 给音频流也注一份（OPUS规范要求）
            for (unsigned int i = 0; i < oc->nb_streams; i++) {
                if (oc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                    av_dict_set(&oc->streams[i]->metadata, "METADATA_BLOCK_PICTURE",
                                base64Cover.c_str(), 0);
                }
            }
        } else {
            // 【流混入模式】：针对 MP3 / M4A / MP4 / FLAC
            AVStream *out_img_stream = avformat_new_stream(oc, nullptr);
            if(!out_img_stream) {
                success = false;
                error = "创建封面输出流失败";
                av_packet_free(&imgPkt);
                goto cleanup;
            }

            ret = avcodec_parameters_copy(out_img_stream->codecpar, in_img_stream->codecpar);
            if(ret < 0) {
                success = false;
                error = QString("复制流的编解码参数失败: %1").arg(getError(ret));
                av_packet_free(&imgPkt);
                goto cleanup;
            }

            out_img_stream->time_base = AVRational{1, 90000}; // 强制净化时间基
            out_img_stream->codecpar->codec_tag = 0;
            out_img_stream->disposition |= AV_DISPOSITION_ATTACHED_PIC;
            image_stream_idx = out_img_stream->index;
        }

        // ================== 3. 写入文件头 ==================
        if(!(oc->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open2(&oc->pb, finalOutUrl.toUtf8().constData(), AVIO_FLAG_WRITE, nullptr, nullptr);
            if(ret < 0) {
                success = false;
                error = QString("打开输出文件失败：%1").arg(getError(ret));
                av_packet_free(&imgPkt);
                goto cleanup;
            }
        }

        if(QString::fromUtf8(oc->oformat->name).contains("mp3"))
            av_dict_set(&oc->metadata, "id3v2_version", "3", 0);

        ret = avformat_write_header(oc, nullptr);
        if(ret < 0) {
            success = false;
            error = QString("写入文件头失败: %1").arg(getError(ret));
            av_packet_free(&imgPkt);
            goto cleanup;
        }


        // 如果是流模式，把那唯一一帧图片 Packet 写进去
        if(!useBase64Mode && image_stream_idx != -1 && gotImage) {
            imgPkt->stream_index = image_stream_idx;
            imgPkt->pts = 0;
            imgPkt->dts = 0;
            imgPkt->duration = 0;
            imgPkt->flags |= AV_PKT_FLAG_KEY;
            imgPkt->pos = -1;

            ret = av_interleaved_write_frame(oc, imgPkt);
            if(ret < 0) {
                success = false;
                error = QString("写入封面数据失败: %1").arg(getError(ret));
                av_packet_free(&imgPkt);
                goto cleanup;
            }
        }
        av_packet_free(&imgPkt);
    }

    // ================== 4. 混入音频包 ==================
    while (av_read_frame(ic_audio, pkt) >= 0) {
        if(stopFlag.load(std::memory_order_acquire)) {
            success = false;
            error = "用户已中止操作";
            break;
        }

        if (streamMapping.contains(pkt->stream_index)) {
            AVStream *in_stream = ic_audio->streams[pkt->stream_index];
            AVStream *out_stream = oc->streams[streamMapping[pkt->stream_index]];
            av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);
            pkt->stream_index = streamMapping[pkt->stream_index];
            pkt->pos = -1;

            ret = av_interleaved_write_frame(oc, pkt);
            if(ret < 0) {
                success = false;
                error = QString("写入音频数据帧失败: %1").arg(getError(ret));
                av_packet_unref(pkt);
                break;
            }
        }
        av_packet_unref(pkt);
    }

    if(success) {
        ret = av_write_trailer(oc);
        if(ret < 0) {
            success = false;
            error = QString("写入文件尾失败: %1").arg(getError(ret));
        }
    }

cleanup:
    if (pkt) av_packet_free(&pkt);
    if (ic_audio) avformat_close_input(&ic_audio);
    if (ic_image) avformat_close_input(&ic_image);
    if (oc && !(oc->oformat->flags & AVFMT_NOFILE)) avio_closep(&oc->pb);
    if (oc) avformat_free_context(oc);

    // 文件安全替换
    std::filesystem::path tmpPath;
#ifdef Q_OS_WIN
    tmpPath = finalOutUrl.toStdWString();
#else
    tmpPath = finalOutUrl.toStdString();
#endif
    if (inPlace) {
        std::filesystem::path backupPath;
        std::filesystem::path inPath;
#ifdef Q_OS_WIN
        backupPath = (inputUrl + ".bak").toStdWString();
        inPath = inputUrl.toStdWString();
#else
        backupPath = (inputUrl + ".bak").toStdString();
        inPath = inputUrl.toStdString();
#endif

        if (success) {
            // 校验临时文件有效性
            if (std::filesystem::file_size(tmpPath, ec) == 0 || ec) {
                success = false;
                error = "生成的临时文件无效";
            } else {
                // 用 FFmpeg 二次校验文件结构完整性
                AVFormatContext* checkCtx = nullptr;
                if (avformat_open_input(&checkCtx, finalOutUrl.toUtf8().constData(), nullptr, nullptr) == 0) {
                    if (avformat_find_stream_info(checkCtx, nullptr) < 0) {
                        success = false;
                        error = "生成的临时文件无效";
                    }
                    avformat_close_input(&checkCtx);
                } else {
                    success = false;
                    error = "生成的临时文件无效";
                }
            }
        }

        if (success) {
            std::filesystem::remove(backupPath, ec);
            std::filesystem::rename(inPath, backupPath, ec);
            if (!ec) {
                std::filesystem::rename(tmpPath, inPath, ec);
                if (!ec) {
                    // 成功替换
                    std::filesystem::remove(backupPath, ec);
                } else {
                    // 替换失败，回滚
                    std::filesystem::rename(backupPath, inPath, ec);
                    std::filesystem::remove(tmpPath, ec);
                    success = false;
                    error = "文件替换失败，已自动回滚";
                }
            } else {
                // 无法备份原文件
                std::filesystem::remove(tmpPath, ec);
                success = false;
                error = "文件可能正在使用";
            }
        } else {
            std::filesystem::remove(tmpPath, ec);
        }
    } else {
        if(!success) {
            std::filesystem::remove(tmpPath, ec);
        }
    }

    return success;
}

QString MediaTool::CheckHDRFormat(AVColorTransferCharacteristic trc, bool isDOVI, bool isHDR10PLUS, bool isHDR10)
{
    // 杜比视界 (Dolby Vision)
    if (isDOVI) {
        return "Dolby Vision";
    }

    // PQ 曲线 (SMPTE ST 2084)
    if(trc == AVCOL_TRC_SMPTE2084 || trc == AVCOL_TRC_SMPTEST2084) {
        if (isHDR10PLUS) {
            return "HDR10+";
        }

        if (isHDR10) {
            return "HDR10";
        }

        return "PQ";
    }

    // HLG 曲线 (ARIB STD-B67)
    if(trc == AVCOL_TRC_ARIB_STD_B67) {
        return "HLG";
    }

    // SDR (标准动态范围)
    return "SDR";
}

void MediaTool::ExtractMetadata(AVDictionary *dict, QMap<QString, QString> &map)
{
    AVDictionaryEntry* tag = nullptr;
    while((tag = av_dict_get(dict, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        map.insert(QString::fromUtf8(tag->key), QString::fromUtf8(tag->value));
    }
}

QString MediaTool::FormatLanguage(const QString &langCode, const QString &title)
{
    // langCode 到中文的映射表
    static const QHash<QString, QString> langMap = {
        {"eng", "英文"}, {"ara", "阿拉伯语"}, {"cat", "加泰罗尼亚语"},
        {"cze", "捷克语"}, {"ces", "捷克语"}, {"dan", "丹麦语"},
        {"ger", "德语"},   {"deu", "德语"},   {"gre", "希腊语"},  {"ell", "希腊语"},
        {"spa", "西班牙语"},{"baq", "巴斯克语"}, {"eus", "巴斯克语"},
        {"fin", "芬兰语"}, {"fil", "菲律宾语"}, {"fre", "法语"},    {"fra", "法语"},
        {"glg", "加利西亚语"},{"heb", "希伯来语"}, {"hin", "印地语"},
        {"hun", "匈牙利语"},{"ind", "印尼语"},   {"ita", "意大利语"},
        {"jpn", "日语"},   {"kan", "卡纳达语"}, {"kor", "韩语"},
        {"mal", "马拉雅拉姆语"},{"may", "马来语"}, {"msa", "马来语"},
        {"nob", "挪威语"}, {"nor", "挪威语"}, {"dut", "荷兰语"}, {"nld", "荷兰语"},
        {"pol", "波兰语"}, {"por", "葡萄牙语"}, {"rum", "罗马尼亚语"}, {"ron", "罗马尼亚语"},
        {"rus", "俄语"},   {"swe", "瑞典语"},   {"tam", "泰米尔语"},
        {"tel", "泰卢固语"},{"tha", "泰语"},     {"tur", "土耳其语"},
        {"ukr", "乌克兰语"},{"vie", "越南语"},   {"chi", "中文"},    {"zho", "中文"},
        {"und", "未知"}
    };

    // Title 到中文的映射表
    static const QHash<QString, QString> titleMap = {
        {"forced", "强制"},
        {"sdh", "听障"},
        {"cc", "闭路字幕"},
        {"european", "欧洲"},
        {"latin american", "拉丁美洲"},
        {"canadian", "加拿大"},
        {"brazilian", "巴西"},
        {"ukraine", "乌克兰"},
        {"vietnam", "越南"},
        {"simplified", "简体"},
        {"traditional", "繁体"}
    };

    QString resultLang;
    QString lowerLang = langCode.toLower().trimmed();

    // 语言代码到中文
    if(langMap.contains(lowerLang)) {
        resultLang = langMap.value(lowerLang);
    } else if(!lowerLang.isEmpty()) {
        resultLang = langCode;
    }

    if(title.isEmpty()) {
        return resultLang;
    }

    // Title 到中文
    QString resultTitle = title.trimmed();
    QString lowerTitle = resultTitle.toLower();

    if(titleMap.contains(lowerTitle)) {
        resultTitle = titleMap.value(lowerTitle);
    } else {
        // 如果包含组合词，进行子串替换
        QHashIterator<QString, QString> i(titleMap);
        while (i.hasNext()) {
            i.next();
            resultTitle.replace(i.key(), i.value(), Qt::CaseInsensitive);
        }
    }

    if(resultLang.isEmpty()) {
        return  QString("(%1)").arg(resultTitle);
    }

    return QString("%1(%2)").arg(resultLang, resultTitle);
}
