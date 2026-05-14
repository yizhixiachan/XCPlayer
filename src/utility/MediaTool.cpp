#include "MediaTool.h"
#include <filesystem>
#include <regex>
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

static std::filesystem::path ToFsPath(const QString& path)
{
#ifdef Q_OS_WIN
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toStdString());
#endif
}

static bool IsRegularFile(const QString &path)
{
    std::error_code ec;
    const auto fsPath = ToFsPath(path);
    return std::filesystem::exists(fsPath, ec) && std::filesystem::is_regular_file(fsPath, ec);
}

static bool CoverMimeFromCodec(AVCodecID codecId, QByteArray& mime)
{
    switch(codecId) {
    case AV_CODEC_ID_PNG:
        mime = "image/png";
        return true;

    case AV_CODEC_ID_MJPEG:
        mime = "image/jpeg";
        return true;

    default:
        return false;
    }
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

    // 循环读取和解码，直到找到一个真正的 I 帧
    while(!gotFrame && maxAttempts-- > 0 && av_read_frame(ic, pkt) >= 0) {
        if(pkt->stream_index == videoStreamIdx) {
            if(avcodec_send_packet(codecCtx, pkt) == 0) {
                while(avcodec_receive_frame(codecCtx, frame) == 0) {
                    // 检查该帧是否为 I 帧
                    if(frame->pict_type == AV_PICTURE_TYPE_I) {
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

    // 如果尝试了1200次还没找到 I 帧，就勉强接受该帧
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
    if(!IsRegularFile(inputUrl)) {
        error = "无效文件";
        return false;
    }

    const bool inPlace = outputUrl.trimmed().isEmpty() || outputUrl == inputUrl;

    QFileInfo fi(inputUrl);
    QString finalOutUrl = outputUrl;
    if(inPlace) {
        const QString ext = fi.suffix().isEmpty() ? ".tmp" : "." + fi.suffix();
        finalOutUrl = fi.absolutePath() + "/" + fi.completeBaseName() + "_tmp" + ext;
    }

    AVFormatContext* ic = nullptr;
    AVFormatContext* oc = nullptr;
    AVPacket* pkt = nullptr;

    QList<int> streamMapping;
    int outStreamIndex = 0;
    bool success = true;
    int ret = 0;

    QByteArray preservedOggCover;

    ic = avformat_alloc_context();
    if(!ic) {
        error = "创建输入上下文失败";
        return false;
    }

    ic->interrupt_callback.callback = Callback;
    ic->interrupt_callback.opaque = &stopFlag;

    ret = avformat_open_input(&ic, inputUrl.toUtf8().constData(), nullptr, nullptr);
    if(ret < 0) {
        error = QString("打开媒体文件失败：%1").arg(FFerror(ret));
        avformat_free_context(ic);
        return false;
    }

    ret = avformat_find_stream_info(ic, nullptr);
    if(ret < 0) {
        error = QString("读取流信息失败：%1").arg(FFerror(ret));
        avformat_close_input(&ic);
        return false;
    }

    ret = avformat_alloc_output_context2(&oc, nullptr, nullptr, finalOutUrl.toUtf8().constData());
    if(!oc || ret < 0) {
        success = false;
        error = QString("创建输出上下文失败：%1").arg(FFerror(ret));
        goto cleanup;
    }

    {
        const bool oggFamily = IsOggFamily(oc);
        streamMapping.fill(-1, static_cast<int>(ic->nb_streams));

        for(unsigned int i = 0; i < ic->nb_streams; ++i) {
            AVStream* inStream = ic->streams[i];

            if(inStream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
                inStream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
                continue;
            }

            // OGG/Opus/Vorbis 不通过 video attached_pic 写封面。
            // 如果 FFmpeg 把旧封面暴露成 attached_pic，这里转回 METADATA_BLOCK_PICTURE。
            if(oggFamily &&
                inStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
                (inStream->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
                QByteArray mime;
                if(inStream->attached_pic.size > 0 &&
                    inStream->attached_pic.data &&
                    CoverMimeFromCodec(inStream->codecpar->codec_id, mime)) {
                    const QByteArray imageData(
                        reinterpret_cast<const char*>(inStream->attached_pic.data),
                        inStream->attached_pic.size
                        );

                    preservedOggCover = MakeVorbisPictureBlock(
                        imageData,
                        mime,
                        inStream->codecpar->width,
                        inStream->codecpar->height
                        );
                }

                continue;
            }

            AVStream* outStream = avformat_new_stream(oc, nullptr);
            if(!outStream) {
                success = false;
                error = "创建输出流失败";
                goto cleanup;
            }

            ret = avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
            if(ret < 0) {
                success = false;
                error = QString("复制流的编解码参数失败：%1").arg(FFerror(ret));
                goto cleanup;
            }

            outStream->time_base = inStream->time_base;
            outStream->disposition = inStream->disposition;

            // 重封装时音频 codec_tag 置 0，避免 muxer 拒绝旧容器 tag。
            if(inStream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                outStream->codecpar->codec_tag = 0;
            } else {
                outStream->codecpar->codec_tag = inStream->codecpar->codec_tag;
            }

            av_dict_copy(&outStream->metadata, inStream->metadata, 0);

            streamMapping[static_cast<int>(i)] = outStreamIndex++;
        }

        if(ic->nb_chapters > 0) {
            oc->chapters = static_cast<AVChapter**>(av_calloc(ic->nb_chapters, sizeof(AVChapter*)));
            if(oc->chapters) {
                oc->nb_chapters = ic->nb_chapters;

                for(unsigned int i = 0; i < ic->nb_chapters; ++i) {
                    AVChapter* inChapter = ic->chapters[i];
                    AVChapter* outChapter = static_cast<AVChapter*>(av_mallocz(sizeof(AVChapter)));
                    if(!outChapter) continue;

                    outChapter->id = inChapter->id;
                    outChapter->time_base = inChapter->time_base;
                    outChapter->start = inChapter->start;
                    outChapter->end = inChapter->end;
                    av_dict_copy(&outChapter->metadata, inChapter->metadata, 0);

                    oc->chapters[i] = outChapter;
                }
            }
        }

        // 非 OGG 主要使用 global metadata。
        // OGG 家族同时同步到音频流 metadata，匹配 VorbisComment 的实际存储位置。
        av_dict_copy(&oc->metadata, ic->metadata, 0);
        ApplyFuzzyMeta(&oc->metadata, metadata);

        if(oggFamily) {
            for(unsigned int i = 0; i < oc->nb_streams; ++i) {
                if(oc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                    ApplyFuzzyMeta(&oc->streams[i]->metadata, metadata);
                }
            }

            if(!preservedOggCover.isEmpty()) {
                SetOggPicture(oc, preservedOggCover);
            }
        }
    }

    if(!(oc->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open2(&oc->pb, finalOutUrl.toUtf8().constData(), AVIO_FLAG_WRITE, nullptr, nullptr);
        if(ret < 0) {
            success = false;
            error = QString("打开输出文件失败：%1").arg(FFerror(ret));
            goto cleanup;
        }
    }

    {
        AVDictionary* muxerOptions = nullptr;
        if(IsMp3Format(oc)) {
            // id3v2_version 是 MP3 muxer option，不是普通 metadata。
            av_dict_set(&muxerOptions, "id3v2_version", "3", 0);
        }

        ret = avformat_write_header(oc, &muxerOptions);
        av_dict_free(&muxerOptions);

        if(ret < 0) {
            success = false;
            error = QString("写入文件头失败：%1").arg(FFerror(ret));
            goto cleanup;
        }
    }

    pkt = av_packet_alloc();
    if(!pkt) {
        success = false;
        error = "创建数据包失败";
        goto cleanup;
    }

    while(true) {
        if(stopFlag.load(std::memory_order_acquire)) {
            success = false;
            error = "用户已中止操作";
            break;
        }

        ret = av_read_frame(ic, pkt);
        if(ret < 0) {
            if(ret == AVERROR_EOF) break;

            success = false;
            error = QString("读取数据帧失败：%1").arg(FFerror(ret));
            break;
        }

        const int inIndex = pkt->stream_index;
        if(inIndex < 0 || inIndex >= streamMapping.size() || streamMapping[inIndex] < 0) {
            av_packet_unref(pkt);
            continue;
        }

        pkt->stream_index = streamMapping[inIndex];

        AVStream* inStream = ic->streams[inIndex];
        AVStream* outStream = oc->streams[pkt->stream_index];

        av_packet_rescale_ts(pkt, inStream->time_base, outStream->time_base);
        pkt->pos = -1;

        ret = av_interleaved_write_frame(oc, pkt);
        if(ret < 0) {
            success = false;
            error = QString("写入数据帧失败：%1").arg(FFerror(ret));
            av_packet_unref(pkt);
            break;
        }

        av_packet_unref(pkt);
    }

    if(success) {
        ret = av_write_trailer(oc);
        if(ret < 0) {
            success = false;
            error = QString("写入文件尾失败：%1").arg(FFerror(ret));
        }
    }

cleanup:
    if(pkt) av_packet_free(&pkt);
    if(ic) avformat_close_input(&ic);
    if(oc && !(oc->oformat->flags & AVFMT_NOFILE)) avio_closep(&oc->pb);
    if(oc) avformat_free_context(oc);

    return FinishFileReplace(inputUrl, finalOutUrl, inPlace, success, error);
}

bool MediaTool::ReplaceCover(const QString &inputUrl, const QString &outputUrl, const QString &imageUrl, QString& error, std::atomic<bool> &stopFlag)
{
    if(!IsRegularFile(inputUrl)) {
        error = "无效文件";
        return false;
    }

    if(!IsRegularFile(imageUrl)) {
        error = "无效图片文件";
        return false;
    }

    const bool inPlace = outputUrl.trimmed().isEmpty() || outputUrl == inputUrl;

    QFileInfo fi(inputUrl);
    QString finalOutUrl = outputUrl;
    if(inPlace) {
        const QString ext = fi.suffix().isEmpty() ? ".tmp" : "." + fi.suffix();
        finalOutUrl = fi.absolutePath() + "/" + fi.completeBaseName() + "_tmp" + ext;
    }

    AVFormatContext* icAudio = nullptr;
    AVFormatContext* icImage = nullptr;
    AVFormatContext* oc = nullptr;
    AVPacket* pkt = nullptr;

    QMap<int, int> streamMapping;
    int imageStreamIndex = -1;
    bool success = true;
    int ret = 0;

    pkt = av_packet_alloc();
    if(!pkt) {
        error = "创建数据包失败";
        return false;
    }

    ret = avformat_open_input(&icAudio, inputUrl.toUtf8().constData(), nullptr, nullptr);
    if(ret < 0) {
        success = false;
        error = QString("打开媒体文件失败：%1").arg(FFerror(ret));
        goto cleanup;
    }

    ret = avformat_find_stream_info(icAudio, nullptr);
    if(ret < 0) {
        success = false;
        error = QString("读取媒体流信息失败：%1").arg(FFerror(ret));
        goto cleanup;
    }

    ret = avformat_open_input(&icImage, imageUrl.toUtf8().constData(), nullptr, nullptr);
    if(ret < 0) {
        success = false;
        error = QString("打开图片文件失败：%1").arg(FFerror(ret));
        goto cleanup;
    }

    ret = avformat_find_stream_info(icImage, nullptr);
    if(ret < 0) {
        success = false;
        error = QString("读取图片流信息失败：%1").arg(FFerror(ret));
        goto cleanup;
    }

    ret = avformat_alloc_output_context2(&oc, nullptr, nullptr, finalOutUrl.toUtf8().constData());
    if(!oc || ret < 0) {
        success = false;
        error = QString("创建输出上下文失败：%1").arg(FFerror(ret));
        goto cleanup;
    }

    {
        const bool oggFamily = IsOggFamily(oc);

        // 封面替换只面向音频文件：复制音频流，丢弃旧 attached_pic 视频流。
        for(unsigned int i = 0; i < icAudio->nb_streams; ++i) {
            AVStream* inStream = icAudio->streams[i];

            if(inStream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
                continue;
            }

            AVStream* outStream = avformat_new_stream(oc, nullptr);
            if(!outStream) {
                success = false;
                error = "创建音频输出流失败";
                goto cleanup;
            }

            ret = avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
            if(ret < 0) {
                success = false;
                error = QString("复制音频流参数失败：%1").arg(FFerror(ret));
                goto cleanup;
            }

            outStream->time_base = inStream->time_base;
            outStream->disposition = inStream->disposition;
            outStream->codecpar->codec_tag = 0;

            av_dict_copy(&outStream->metadata, inStream->metadata, 0);

            // OGG 旧封面在 VorbisComment 中，先清掉，稍后写入新块。
            if(oggFamily) {
                av_dict_set(&outStream->metadata, "METADATA_BLOCK_PICTURE", nullptr, 0);
                av_dict_set(&outStream->metadata, "COVERART", nullptr, 0);
            }

            streamMapping[static_cast<int>(i)] = outStream->index;
        }

        if(streamMapping.isEmpty()) {
            success = false;
            error = "未找到可写入的音频流";
            goto cleanup;
        }

        if(icAudio->nb_chapters > 0) {
            oc->chapters = static_cast<AVChapter**>(av_calloc(icAudio->nb_chapters, sizeof(AVChapter*)));
            if(oc->chapters) {
                oc->nb_chapters = icAudio->nb_chapters;

                for(unsigned int i = 0; i < icAudio->nb_chapters; ++i) {
                    AVChapter* inChapter = icAudio->chapters[i];
                    AVChapter* outChapter = static_cast<AVChapter*>(av_mallocz(sizeof(AVChapter)));
                    if(!outChapter) continue;

                    outChapter->id = inChapter->id;
                    outChapter->time_base = inChapter->time_base;
                    outChapter->start = inChapter->start;
                    outChapter->end = inChapter->end;
                    av_dict_copy(&outChapter->metadata, inChapter->metadata, 0);

                    oc->chapters[i] = outChapter;
                }
            }
        }

        av_dict_copy(&oc->metadata, icAudio->metadata, 0);

        if(oggFamily) {
            ClearOggPicture(oc);
        }

        AVPacket* imgPkt = av_packet_alloc();
        if(!imgPkt) {
            success = false;
            error = "创建图片数据包失败";
            goto cleanup;
        }

        AVStream* imgStream = nullptr;
        bool gotImage = false;

        while(av_read_frame(icImage, imgPkt) >= 0) {
            AVStream* candidate = icImage->streams[imgPkt->stream_index];
            if(candidate->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                imgStream = candidate;
                gotImage = true;
                break;
            }

            av_packet_unref(imgPkt);
        }

        if(!gotImage || !imgStream || imgPkt->size <= 0) {
            av_packet_free(&imgPkt);
            success = false;
            error = "无效图像数据";
            goto cleanup;
        }

        QByteArray mime;
        if(!CoverMimeFromCodec(imgStream->codecpar->codec_id, mime)) {
            av_packet_free(&imgPkt);
            success = false;
            error = "仅支持 PNG 或 JPEG 封面图片";
            goto cleanup;
        }

        if(oggFamily) {
            const QByteArray imageData(reinterpret_cast<const char*>(imgPkt->data), imgPkt->size);

            const QByteArray base64Picture = MakeVorbisPictureBlock(
                imageData,
                mime,
                imgStream->codecpar->width,
                imgStream->codecpar->height
            );

            SetOggPicture(oc, base64Picture);
        } else {
            AVStream* outImageStream = avformat_new_stream(oc, nullptr);
            if(!outImageStream) {
                av_packet_free(&imgPkt);
                success = false;
                error = "创建封面输出流失败";
                goto cleanup;
            }

            ret = avcodec_parameters_copy(outImageStream->codecpar, imgStream->codecpar);
            if(ret < 0) {
                av_packet_free(&imgPkt);
                success = false;
                error = QString("复制封面流参数失败：%1").arg(FFerror(ret));
                goto cleanup;
            }

            outImageStream->time_base = AVRational{1, 90000};
            outImageStream->codecpar->codec_tag = 0;
            outImageStream->disposition |= AV_DISPOSITION_ATTACHED_PIC;
            imageStreamIndex = outImageStream->index;
        }

        if(!(oc->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open2(&oc->pb, finalOutUrl.toUtf8().constData(), AVIO_FLAG_WRITE, nullptr, nullptr);
            if(ret < 0) {
                av_packet_free(&imgPkt);
                success = false;
                error = QString("打开输出文件失败：%1").arg(FFerror(ret));
                goto cleanup;
            }
        }

        {
            AVDictionary* muxerOptions = nullptr;
            if(IsMp3Format(oc)) {
                // MP3 的 ID3v2 版本是 muxer option，不是普通 metadata。
                av_dict_set(&muxerOptions, "id3v2_version", "3", 0);
            }

            ret = avformat_write_header(oc, &muxerOptions);
            av_dict_free(&muxerOptions);

            if(ret < 0) {
                av_packet_free(&imgPkt);
                success = false;
                error = QString("写入文件头失败：%1").arg(FFerror(ret));
                goto cleanup;
            }
        }

        if(!oggFamily && imageStreamIndex >= 0) {
            imgPkt->stream_index = imageStreamIndex;
            imgPkt->pts = 0;
            imgPkt->dts = 0;
            imgPkt->duration = 0;
            imgPkt->flags |= AV_PKT_FLAG_KEY;
            imgPkt->pos = -1;

            ret = av_interleaved_write_frame(oc, imgPkt);
            if(ret < 0) {
                av_packet_free(&imgPkt);
                success = false;
                error = QString("写入封面数据失败：%1").arg(FFerror(ret));
                goto cleanup;
            }
        }

        av_packet_free(&imgPkt);
    }

    while(av_read_frame(icAudio, pkt) >= 0) {
        if(stopFlag.load(std::memory_order_acquire)) {
            success = false;
            error = "用户已中止操作";
            break;
        }

        if(streamMapping.contains(pkt->stream_index)) {
            AVStream* inStream = icAudio->streams[pkt->stream_index];
            AVStream* outStream = oc->streams[streamMapping[pkt->stream_index]];

            av_packet_rescale_ts(pkt, inStream->time_base, outStream->time_base);
            pkt->stream_index = streamMapping[pkt->stream_index];
            pkt->pos = -1;

            ret = av_interleaved_write_frame(oc, pkt);
            if(ret < 0) {
                success = false;
                error = QString("写入音频数据帧失败：%1").arg(FFerror(ret));
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
            error = QString("写入文件尾失败：%1").arg(FFerror(ret));
        }
    }

cleanup:
    if(pkt) av_packet_free(&pkt);
    if(icAudio) avformat_close_input(&icAudio);
    if(icImage) avformat_close_input(&icImage);
    if(oc && !(oc->oformat->flags & AVFMT_NOFILE)) avio_closep(&oc->pb);
    if(oc) avformat_free_context(oc);

    return FinishFileReplace(inputUrl, finalOutUrl, inPlace, success, error);
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

QString MediaTool::FFerror(int ret)
{
    char errbuf[1024] = {0};
    av_strerror(ret, errbuf, sizeof(errbuf));
    return QString::fromUtf8(errbuf);
}

bool MediaTool::IsOggFamily(const AVFormatContext *ctx)
{
    if(!ctx || !ctx->oformat || !ctx->oformat->name) return false;

    const QString name = QString::fromUtf8(ctx->oformat->name).toLower();
    return name.contains("ogg") || name.contains("oga") || name.contains("opus");
}

bool MediaTool::IsMp3Format(const AVFormatContext *ctx)
{
    if(!ctx || !ctx->oformat || !ctx->oformat->name) return false;

    const QString name = QString::fromUtf8(ctx->oformat->name).toLower();
    return name.contains("mp3");
}

void MediaTool::WriteBe32(std::vector<uint8_t> &buf, uint32_t value)
{
    buf.push_back((value >> 24) & 0xFF);
    buf.push_back((value >> 16) & 0xFF);
    buf.push_back((value >> 8) & 0xFF);
    buf.push_back(value & 0xFF);
}

QByteArray MediaTool::MakeVorbisPictureBlock(const QByteArray &imageData, const QByteArray &mime, int width, int height)
{
    std::vector<uint8_t> block;

    WriteBe32(block, 3); // 3 = Front Cover
    WriteBe32(block, static_cast<uint32_t>(mime.size()));
    block.insert(block.end(), mime.begin(), mime.end());

    WriteBe32(block, 0); // description length, UTF-8, empty

    // VorbisComment/FLAC picture block 要求这些字段准确或置 0。
    // width/height 来自 FFmpeg，色深和索引色数这里不深度解析，因此置 0。
    WriteBe32(block, width > 0 ? static_cast<uint32_t>(width) : 0);
    WriteBe32(block, height > 0 ? static_cast<uint32_t>(height) : 0);
    WriteBe32(block, 0); // color depth unknown
    WriteBe32(block, 0); // indexed colors unknown

    WriteBe32(block, static_cast<uint32_t>(imageData.size()));
    block.insert(block.end(), imageData.begin(), imageData.end());

    return QByteArray(reinterpret_cast<const char*>(block.data()), static_cast<int>(block.size())).toBase64();
}

void MediaTool::ClearOggPicture(AVFormatContext *ctx)
{
    if(!ctx) return;

    av_dict_set(&ctx->metadata, "METADATA_BLOCK_PICTURE", nullptr, 0);
    av_dict_set(&ctx->metadata, "COVERART", nullptr, 0);

    for(unsigned int i = 0; i < ctx->nb_streams; ++i) {
        av_dict_set(&ctx->streams[i]->metadata, "METADATA_BLOCK_PICTURE", nullptr, 0);
        av_dict_set(&ctx->streams[i]->metadata, "COVERART", nullptr, 0);
    }
}

void MediaTool::SetOggPicture(AVFormatContext *ctx, const QByteArray &base64Picture)
{
    if(!ctx || base64Picture.isEmpty()) return;

    av_dict_set(&ctx->metadata, "METADATA_BLOCK_PICTURE", base64Picture.constData(), 0);

    for(unsigned int i = 0; i < ctx->nb_streams; ++i) {
        AVStream *st = ctx->streams[i];
        if(st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            av_dict_set(&st->metadata, "METADATA_BLOCK_PICTURE", base64Picture.constData(), 0);
        }
    }
}

void MediaTool::ApplyFuzzyMeta(AVDictionary **dict, const QMap<QString, QString> &newMeta)
{
    for(auto it = newMeta.constBegin(); it != newMeta.constEnd(); ++it) {
        QString targetKey = it.key();
        const QString val = it.value().trimmed();

        AVDictionaryEntry *tag = nullptr;
        bool found = false;

        while((tag = av_dict_get(*dict, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            if(QString::fromUtf8(tag->key).compare(targetKey, Qt::CaseInsensitive) == 0) {
                targetKey = QString::fromUtf8(tag->key);
                found = true;
                break;
            }
        }

        if(!found && targetKey.compare("lyrics", Qt::CaseInsensitive) == 0) {
            tag = nullptr;
            while((tag = av_dict_get(*dict, "", tag, AV_DICT_IGNORE_SUFFIX))) {
                if(QString::fromUtf8(tag->key).startsWith("lyric", Qt::CaseInsensitive)) {
                    targetKey = QString::fromUtf8(tag->key);
                    break;
                }
            }
        }

        if(val.isEmpty()) {
            av_dict_set(dict, targetKey.toUtf8().constData(), nullptr, 0);
        } else {
            av_dict_set(dict, targetKey.toUtf8().constData(), val.toUtf8().constData(), 0);
        }
    }
}

bool MediaTool::ValidateGeneratedFile(const QString &path, QString &error)
{
    std::error_code ec;
    const auto fsPath = ToFsPath(path);

    if(std::filesystem::file_size(fsPath, ec) == 0 || ec) {
        error = "生成的临时文件无效";
        return false;
    }

    AVFormatContext *checkCtx = nullptr;
    int ret = avformat_open_input(&checkCtx, path.toUtf8().constData(), nullptr, nullptr);
    if(ret < 0) {
        error = "生成的临时文件无效";
        return false;
    }

    ret = avformat_find_stream_info(checkCtx, nullptr);
    avformat_close_input(&checkCtx);

    if(ret < 0) {
        error = "生成的临时文件无效";
        return false;
    }

    return true;
}

bool MediaTool::FinishFileReplace(const QString &inputUrl, const QString &finalOutUrl, bool inPlace, bool success, QString &error)
{
    std::error_code ec;
    const auto tmpPath = ToFsPath(finalOutUrl);

    if(inPlace) {
        if(success && !ValidateGeneratedFile(finalOutUrl, error)) {
            success = false;
        }

        if(success) {
            const auto inPath = ToFsPath(inputUrl);
            const auto backupPath = ToFsPath(inputUrl + ".bak");

            std::filesystem::remove(backupPath, ec);

            ec.clear();
            std::filesystem::rename(inPath, backupPath, ec);
            if(ec) {
                std::filesystem::remove(tmpPath, ec);
                error = "文件可能正在使用";
                return false;
            }

            ec.clear();
            std::filesystem::rename(tmpPath, inPath, ec);
            if(ec) {
                std::filesystem::rename(backupPath, inPath, ec);
                std::filesystem::remove(tmpPath, ec);
                error = "文件替换失败，已自动回滚";
                return false;
            }

            std::filesystem::remove(backupPath, ec);
            return true;
        }

        std::filesystem::remove(tmpPath, ec);
        return false;
    }

    if(!success) {
        std::filesystem::remove(tmpPath, ec);
    }

    return success;
}
