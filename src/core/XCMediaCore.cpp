#include "XCMediaCore.h"
#include "XCAudioEngine.h"
#include <regex>
extern "C"
{
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_d3d11va.h"
#include "libavutil/imgutils.h"
}

static enum AVPixelFormat get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts)
{
    const enum AVPixelFormat* p;

    // 尝试寻找对应的硬件像素格式
    if(ctx->hw_device_ctx) {
        AVHWDeviceContext *device_ctx = (AVHWDeviceContext*)(ctx->hw_device_ctx->data);
        enum AVHWDeviceType type = device_ctx->type;
        for(p = pix_fmts; *p != -1; p++) {
            if(type == AV_HWDEVICE_TYPE_D3D11VA && *p == AV_PIX_FMT_D3D11) {
                return *p;
            }
        }
    }

    // 找不到硬件像素格式，回退到软件像素格式
    return pix_fmts[0];
}

XCMediaCore::XCMediaCore(std::function<void()> onPlayCompleted)
    : onPlayCompleted(onPlayCompleted)
{
    audioDecoder = std::make_unique<Decoder>();
    videoDecoder = std::make_unique<Decoder>();
    subtitleDecoder = std::make_unique<Decoder>();
    audioEngine = std::make_unique<XCAudioEngine>();

    audioClock.Init(&globalSerial);
    videoClock.Init(&globalSerial);

    audioEngine->SetPlayEndCallback(onPlayCompleted);
    audioEngine->SetConsumeCallback([this]() { waitCV.notify_all(); });
    this->onPlayCompleted = onPlayCompleted;

    av_log_set_level(AV_LOG_QUIET);
}

XCMediaCore::~XCMediaCore()
{
    Stop();
    if (hwdeviceCtx) {
        av_buffer_unref(&hwdeviceCtx);
    }
}

int XCMediaCore::Play(const std::string& url, AVDictionary* opts)
{
    Stop();

    audioDecoder->Reset();
    videoDecoder->Reset();
    subtitleDecoder->Reset();

    bStopDemux = false;
    bStopAudio = false;
    bStopVideo = false;
    bStopSubtitle = false;
    bPauseReq = false;
    bSeekReq = false;
    seekTarget = 0.0;
    bSpeedReq = false;
    bStepReq = false;
    isLive = false;
    startTime = 0.0;

    audioStreamIndex = -1;
    videoStreamIndex = -1;
    audioStreamIndex = -1;

    globalSerial = 0;
    latestPacketPts = 0;

    AVFormatContext* ic = avformat_alloc_context();

    auto callback =[](void* opaque) {
        auto* bStopReq = static_cast<std::atomic<bool>*>(opaque);
        if(bStopReq && bStopReq->load(std::memory_order_acquire)) return 1;
        return 0;
    };
    ic->interrupt_callback.callback = callback;
    ic->interrupt_callback.opaque = &bStopDemux;

    int ret = avformat_open_input(&ic, url.c_str(), nullptr, &opts);
    av_dict_free(&opts);
    if(ret < 0) {
        if(ret == AVERROR_HTTP_BAD_REQUEST) return -2;
        if(ret == AVERROR_HTTP_UNAUTHORIZED) return -3;
        if(ret == AVERROR_HTTP_FORBIDDEN) return -4;
        if(ret == AVERROR_HTTP_NOT_FOUND) return -5;
        if(ret == AVERROR_HTTP_TOO_MANY_REQUESTS) return -6;
        if(ret == AVERROR_HTTP_OTHER_4XX) return -7;
        return -1;
    }

    if(avformat_find_stream_info(ic, nullptr) < 0) {
        avformat_close_input(&ic);
        return -1;
    }

    if(ic->start_time != AV_NOPTS_VALUE) {
        startTime = (double)ic->start_time / AV_TIME_BASE;
    }

    isLive = IsLiveStream(ic);

    int bestAudio = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, audioStreamIndex, -1, nullptr, 0);
    int bestVideo = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, videoStreamIndex, -1, nullptr, 0);
    int bestSubtitle = av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE, subtitleStreamIndex, -1, nullptr, 0);

    if(bestVideo >= 0) {
        if(ic->streams[bestVideo]->disposition & AV_DISPOSITION_ATTACHED_PIC) bestVideo = -1;
    }


    audioStreamIndex = bestAudio;
    videoStreamIndex = bestVideo;
    subtitleStreamIndex = bestSubtitle;

    inputCtxPtr.reset(ic);

    demuxThread = std::thread(&XCMediaCore::DemuxLoop, this);

    return 1;
}

void XCMediaCore::SetPause(bool pause)
{
    if(bPauseReq.exchange(pause, std::memory_order_acq_rel) == pause) return;

    audioEngine->SetPause(pause);
    audioClock.Pause(pause);
    videoClock.Pause(pause);
    externalClock.Pause(pause);

    if(!pause) {
        if(isLive) {
            // 从暂停恢复直播流时，抛弃旧数据，播放最新画面
            globalSerial.fetch_add(1, std::memory_order_acq_rel);
            bSeekReq.store(true, std::memory_order_release);
        }
        waitCV.notify_all();
    }
}

void XCMediaCore::Seek(double sec) {
    if(!inputCtxPtr || isLive) return;

    double duration = 0.0;
    if(inputCtxPtr->duration != AV_NOPTS_VALUE) {
        duration = inputCtxPtr->duration * av_q2d(AV_TIME_BASE_Q);
    }

    if(duration <= 0.0) return;

    seekTarget.store(std::clamp(sec, 0.0, duration), std::memory_order_relaxed);

    audioClock.Set(seekTarget.load(std::memory_order_relaxed), globalSerial.load(std::memory_order_acquire));
    videoClock.Set(seekTarget.load(std::memory_order_relaxed), globalSerial.load(std::memory_order_acquire));
    externalClock.Set(seekTarget.load(std::memory_order_relaxed), globalSerial.load(std::memory_order_acquire));

    globalSerial.fetch_add(1, std::memory_order_acq_rel);

    bSeekReq.store(true, std::memory_order_release);
    bStepReq.store(true, std::memory_order_release);

    audioDecoder->Wake();
    videoDecoder->Wake();
    subtitleDecoder->Wake();
    waitCV.notify_all();
}

void XCMediaCore::Abort()
{
    bStopDemux.store(true, std::memory_order_release);
    bStopAudio.store(true, std::memory_order_release);
    bStopVideo.store(true, std::memory_order_release);
    bStopSubtitle.store(true, std::memory_order_release);

    audioEngine->Abort();

    if(audioDecoder) audioDecoder->Wake();
    if(videoDecoder) videoDecoder->Wake();
    if(subtitleDecoder) subtitleDecoder->Wake();
    waitCV.notify_all();
}

void XCMediaCore::Stop()
{
    Abort();
    audioClock.Reset();
    videoClock.Reset();
    externalClock.Reset();
    if(videoRenderer) videoRenderer->Clear();
    if(demuxThread.joinable()) demuxThread.join();
    if(videoRenderThread.joinable()) videoRenderThread.join();
    if(videoDecodeThread.joinable()) videoDecodeThread.join();
    if(audioDecodeThread.joinable()) audioDecodeThread.join();
    if(subtitleDecodeThread.joinable()) subtitleDecodeThread.join();
}

void XCMediaCore::OpenAudioStream(int index) {
    if (audioStreamIndex.load() == index) return;
    audioStreamIndex.store(index);
    Seek(GetMasterClock());
}

void XCMediaCore::OpenVideoStream(int index) {
    if (videoStreamIndex.load() == index) return;
    videoStreamIndex.store(index);
    Seek(GetMasterClock());
}

void XCMediaCore::OpenSubtitleStream(int index) {
    if (subtitleStreamIndex.load() == index) return;
    subtitleStreamIndex.store(index);
    Seek(GetMasterClock());
}

void XCMediaCore::SetSpeed(float multiplier) {
    if(speedTarget.exchange(multiplier, std::memory_order_acq_rel) == multiplier) return;
    bSpeedReq.store(true, std::memory_order_release);
    audioClock.SetSpeed(multiplier);
    videoClock.SetSpeed(multiplier);
    externalClock.SetSpeed(multiplier);
}

float XCMediaCore::GetSpeed() const { return speedTarget.load(std::memory_order_acquire); }

double XCMediaCore::GetMasterClock() const {
    if(IsIdle()) return 0.0;

    double pts = 0.0;
    switch(syncMode) {
    case AudioSync: pts = audioClock.Get(); break;
    case VideoSync: pts = videoClock.Get(); break;
    case ExternalSync: pts = externalClock.Get(); break;
    default: pts = audioClock.Get(); break;
    }
    return pts < 0.0 ? 0.0 : pts;
}

void XCMediaCore::SetHWAccelEnabled(bool enable) {
    if(bHWAccel.exchange(enable, std::memory_order_acq_rel) == enable) return;
    if(IsIdle()) return;
    bHWAccelChanged.store(true, std::memory_order_release);
    Seek(GetMasterClock());
}

void XCMediaCore::SetVolume(float value) { audioEngine->SetVolume(value); }

float XCMediaCore::GetVolume() const { return audioEngine->GetVolume(); }

void XCMediaCore::SetMute(bool mute) { audioEngine->SetMute(mute); }

bool XCMediaCore::IsMute() const { return audioEngine->IsMute(); }

void XCMediaCore::SetExclusiveMode(bool exclusive) { audioEngine->SetExclusiveMode(exclusive); }

void XCMediaCore::SetReplayGainEnabled(bool enable) { audioEngine->SetReplayGainEnabled(enable); }

void XCMediaCore::DemuxLoop() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    bool bEOF = false;
    int currentAudioStreamIndex = audioStreamIndex.load();
    int currentVideoStreamIndex = videoStreamIndex.load();
    int currentSubtitleStreamIndex = subtitleStreamIndex.load();

    if(currentAudioStreamIndex >= 0) syncMode = AudioSync;
    else if(currentVideoStreamIndex >= 0) syncMode = VideoSync;
    else return;

    // 更新最新包的PTS
    auto updateLatestPacketPts= [&](AVPacket* p) {
        double pts = 0.0;
        if(p->pts != AV_NOPTS_VALUE) {
            if (p->stream_index == currentAudioStreamIndex && audioDecoder) {
                pts = p->pts * av_q2d(audioDecoder->time_base) - startTime;
            } else if (p->stream_index == currentVideoStreamIndex && videoDecoder) {
                pts = p->pts * av_q2d(videoDecoder->time_base) - startTime;
            }
        }
        if(pts > latestPacketPts.load(std::memory_order_acquire)) {
            latestPacketPts.store(pts, std::memory_order_release);
        }
    };

    auto initAudio = [&](int index) {
        AVCodecContext* avctx = avcodec_alloc_context3(nullptr);
        audioDecoder->avctxPtr = AVCodecContextPtr(avctx);
        avcodec_parameters_to_context(audioDecoder->avctxPtr.get(), inputCtxPtr->streams[index]->codecpar);
        const AVCodec* acodec = avcodec_find_decoder(audioDecoder->avctxPtr->codec_id);
        if(acodec && avcodec_open2(audioDecoder->avctxPtr.get(), acodec, nullptr) >= 0) {
            audioDecoder->time_base = inputCtxPtr->streams[index]->time_base;
            bStopAudio = false;
            audioDecodeThread = std::thread(&XCMediaCore::AudioDecodeLoop, this);
            audioEngine->Start(audioDecoder.get(), &audioClock);
            return true;
        }
        return false;
    };

    auto initVideo = [&](int index) {
        AVCodecContext* avctx = avcodec_alloc_context3(nullptr);
        bool useHW = bHWAccel.load(std::memory_order_acquire);
        if(!useHW) avctx->thread_count = std::min(8u, std::thread::hardware_concurrency());
        videoDecoder->avctxPtr = AVCodecContextPtr(avctx);
        avcodec_parameters_to_context(videoDecoder->avctxPtr.get(), inputCtxPtr->streams[index]->codecpar);
        const AVCodec* vcodec = avcodec_find_decoder(videoDecoder->avctxPtr->codec_id);
        if(useHW && hwdeviceCtx) {
            videoDecoder->avctxPtr->hw_device_ctx = av_buffer_ref(hwdeviceCtx);
            videoDecoder->avctxPtr->get_format = get_hw_format;
            videoDecoder->avctxPtr->extra_hw_frames = 8;
        }
        if(vcodec && avcodec_open2(videoDecoder->avctxPtr.get(), vcodec, nullptr) >= 0) {
            videoDecoder->time_base = inputCtxPtr->streams[index]->time_base;
            bStopVideo = false;
            videoDecodeThread = std::thread(&XCMediaCore::VideoDecodeLoop, this);
            videoRenderThread = std::thread(&XCMediaCore::VideoRenderLoop, this);
            return true;
        }
        return false;
    };

    auto initSubtitle = [&](int index) {
        AVCodecContext* avctx = avcodec_alloc_context3(nullptr);
        subtitleDecoder->avctxPtr = AVCodecContextPtr(avctx);
        avcodec_parameters_to_context(subtitleDecoder->avctxPtr.get(), inputCtxPtr->streams[index]->codecpar);
        const AVCodec* scodec = avcodec_find_decoder(subtitleDecoder->avctxPtr->codec_id);
        if(scodec && avcodec_open2(subtitleDecoder->avctxPtr.get(), scodec, nullptr) >= 0) {
            subtitleDecoder->time_base = inputCtxPtr->streams[index]->time_base;
            bStopSubtitle = false;
            subtitleDecodeThread = std::thread(&XCMediaCore::SubtitleDecodeLoop, this);
            return true;
        }
        return false;
    };

    if(currentAudioStreamIndex >= 0) initAudio(currentAudioStreamIndex);
    if(currentVideoStreamIndex >= 0) initVideo(currentVideoStreamIndex);
    if(currentSubtitleStreamIndex >= 0) initSubtitle(currentSubtitleStreamIndex);

    while(!bStopDemux.load(std::memory_order_acquire)) {
        // 音频流切换请求
        int reqAudio = audioStreamIndex.load(std::memory_order_acquire);
        if(reqAudio != currentAudioStreamIndex) {
            bStopAudio.store(true, std::memory_order_release);
            audioDecoder->Wake();
            if(audioDecodeThread.joinable()) audioDecodeThread.join();
            audioEngine->Stop();
            audioDecoder->Reset();
            if(reqAudio >= 0 && initAudio(reqAudio)) currentAudioStreamIndex = reqAudio;
            else { currentAudioStreamIndex = -1; audioStreamIndex.store(-1, std::memory_order_release); }
        }

        // 视频流切换请求或硬件加速切换请求
        int reqVideo = videoStreamIndex.load(std::memory_order_acquire);
        bool hwChanged = bHWAccelChanged.exchange(false, std::memory_order_acq_rel);
        if(reqVideo != currentVideoStreamIndex || hwChanged) {
            bStopVideo.store(true, std::memory_order_release);
            videoDecoder->Wake();
            if (videoRenderThread.joinable()) videoRenderThread.join();
            if (videoDecodeThread.joinable()) videoDecodeThread.join();
            videoDecoder->Reset();
            if(videoRenderer) videoRenderer->Clear();
            int targetStream = (reqVideo != currentVideoStreamIndex) ? reqVideo : currentVideoStreamIndex;
            if(targetStream >= 0 && initVideo(targetStream)) currentVideoStreamIndex = targetStream;
            else { currentVideoStreamIndex = -1; videoStreamIndex.store(-1, std::memory_order_release); }
        }

        // 字幕流切换请求
        int reqSub = subtitleStreamIndex.load(std::memory_order_acquire);
        if(reqSub != currentSubtitleStreamIndex) {
            bStopSubtitle = true;
            subtitleDecoder->Wake();
            if(subtitleDecodeThread.joinable()) subtitleDecodeThread.join();
            subtitleDecoder->Reset();
            if(reqSub >= 0 && initSubtitle(reqSub)) currentSubtitleStreamIndex = reqSub;
            else { currentSubtitleStreamIndex = -1; subtitleStreamIndex.store(-1, std::memory_order_release); }
        }

        // Seek 请求
        if(bSeekReq.load(std::memory_order_acquire)) {
            if(!isLive) {
                int64_t target = (int64_t)((seekTarget.load() + startTime) * AV_TIME_BASE);
                avformat_seek_file(inputCtxPtr.get(), -1, INT64_MIN, target, INT64_MAX, AVSEEK_FLAG_BACKWARD);
            }

            if(currentAudioStreamIndex >= 0) {
                audioEngine->Flush();
                audioDecoder->Flush();
                PacketUnit unit{nullptr, true, 0, globalSerial.load(std::memory_order_acquire)};
                audioDecoder->pkt_queue.Push(std::move(unit));
            }
            if(currentVideoStreamIndex >= 0) {
                videoDecoder->Flush();
                PacketUnit unit{nullptr, true, 0, globalSerial.load(std::memory_order_acquire)};
                videoDecoder->pkt_queue.Push(std::move(unit));
            }
            if(currentSubtitleStreamIndex >= 0) {
                subtitleDecoder->Flush();
                PacketUnit unit{nullptr, true, 0, globalSerial.load(std::memory_order_acquire)};
                subtitleDecoder->pkt_queue.Push(std::move(unit));
            }

            bEOF = false;

            latestPacketPts.store(0.0, std::memory_order_release);
            bSeekReq.store(false, std::memory_order_release);
        }

        // 读到文件末尾，休眠解复用线程
        if(bEOF) {
            std::unique_lock<std::mutex> lock(waitMutex);
            waitCV.wait(lock, [this]{ return bSeekReq.load() || bStopDemux.load(std::memory_order_acquire); });
            if(bStopDemux.load(std::memory_order_acquire)) break;
            continue;
        }

        // 流控阻塞
        if(HasEnoughPackets()) {
            std::unique_lock<std::mutex> lock(waitMutex);
            waitCV.wait(lock, [this]{
                return !HasEnoughPackets() ||
                       bSeekReq.load(std::memory_order_acquire) ||
                       bStopDemux.load(std::memory_order_acquire);
            });

            if(bStopDemux.load(std::memory_order_acquire)) break;
            continue;
        }

        // 读取未解码的音视频数据包
        AVPacketPtr pkt(av_packet_alloc());
        int ret = av_read_frame(inputCtxPtr.get(), pkt.get());

        if(ret < 0) {
            if((ret == AVERROR_EOF || avio_feof(inputCtxPtr->pb)) && !bEOF) {
                if(currentAudioStreamIndex >= 0) audioDecoder->pkt_queue.Push({nullptr, false});
                if(currentVideoStreamIndex >= 0) videoDecoder->pkt_queue.Push({nullptr, false});
                if(currentSubtitleStreamIndex >= 0) subtitleDecoder->pkt_queue.Push({nullptr, false});
                bEOF = true;
            }
            continue;
        }

        updateLatestPacketPts(pkt.get());

        PacketUnit unit { std::move(pkt), false };
        unit.data_size = unit.pkt->size;
        unit.serial = globalSerial.load(std::memory_order_acquire);

        if(unit.pkt->stream_index == currentAudioStreamIndex) {
            audioDecoder->pkt_queue.Push(std::move(unit));
        } else if(unit.pkt->stream_index == currentVideoStreamIndex) {
            videoDecoder->pkt_queue.Push(std::move(unit));
        } else if(unit.pkt->stream_index == currentSubtitleStreamIndex) {
            subtitleDecoder->pkt_queue.Push(std::move(unit));
        }
    }
}

void XCMediaCore::AudioDecodeLoop() {
    InitAudioFilter(speedTarget.load(std::memory_order_acquire));
    float curSpeed = speedTarget.load(std::memory_order_acquire);

    PacketUnit unit;
    bool has_pending = false;
    double next_audio_pts = 0.0;

    double filter_out_media_pts = 0.0;
    bool bFirstFilterOutput = true;
    int currentSerial = 0;

    while(!bStopAudio.load(std::memory_order_acquire)) {
        while(!bStopAudio.load(std::memory_order_acquire)) {
            // 接收解码后音频帧
            AVFramePtr frame(av_frame_alloc());
            int ret = avcodec_receive_frame(audioDecoder->avctxPtr.get(), frame.get());
            if(ret == AVERROR(EAGAIN)) break;   // 数据包不够了，需要送包
            if(ret == AVERROR_EOF) {            // 流结束
                audioDecoder->bFinished = true;
                break;
            }
            if(ret < 0) break;

            // 计算音频帧的PTS
            double pts = 0.0;
            if(frame->pts != AV_NOPTS_VALUE) {              // 优先使用自带 Pts
                pts = frame->pts * av_q2d(audioDecoder->time_base) - startTime;
                next_audio_pts = pts + ((double)frame->nb_samples / frame->sample_rate);
            } else if(frame->pkt_dts != AV_NOPTS_VALUE) {   // 其次 pkt_dts
                pts = frame->pkt_dts * av_q2d(audioDecoder->time_base) - startTime;
                next_audio_pts = pts + ((double)frame->nb_samples / frame->sample_rate);
            } else {                                        // 最后推算
                pts = next_audio_pts;
                next_audio_pts += ((double)frame->nb_samples / frame->sample_rate);
            }

            // 丢弃过时帧
            double target = seekTarget.load(std::memory_order_acquire);
            if(pts < target - 0.1) continue;

            // 音频变速处理
            if(audioDecoder->filter_src_ctx && audioDecoder->filter_sink_ctx) {
                // 将改帧送入滤镜输入端
                if(av_buffersrc_add_frame(audioDecoder->filter_src_ctx, frame.get()) >= 0) {
                    // 获取滤镜输出端帧
                    while(true) {
                        AVFramePtr filt_frame(av_frame_alloc());
                        if(av_buffersink_get_frame(audioDecoder->filter_sink_ctx, filt_frame.get()) < 0) break;

                        if(bFirstFilterOutput || std::fabs(filter_out_media_pts - pts) > 0.1) {
                            filter_out_media_pts = pts;
                            bFirstFilterOutput = false;
                        }

                        // 计算变速后的帧时长
                        double media_duration = (double)filt_frame->nb_samples / filt_frame->sample_rate * curSpeed;

                        // 推入帧队列
                        FrameUnit f;
                        f.pts = filter_out_media_pts;
                        filter_out_media_pts += media_duration;
                        f.duration = media_duration;
                        f.speed = curSpeed;

                        f.data_size = av_samples_get_buffer_size(nullptr, filt_frame->ch_layout.nb_channels, filt_frame->nb_samples,
                                                                 (AVSampleFormat)filt_frame->format, 1);
                        if(f.data_size <= 0) f.data_size = 1024;
                        f.frame = std::move(filt_frame);
                        f.serial = currentSerial;

                        audioDecoder->frame_queue.Push(std::move(f));
                    }
                }
            }
            else {
                // 直接推入原始帧
                FrameUnit f;
                f.pts = pts;
                f.duration = (double)frame->nb_samples / frame->sample_rate;
                f.speed = 1.0;
                f.data_size = av_samples_get_buffer_size(nullptr, frame->ch_layout.nb_channels, frame->nb_samples, (AVSampleFormat)frame->format, 1);
                if(f.data_size <= 0) f.data_size = 1024;
                f.frame = std::move(frame);
                f.serial = currentSerial;

                audioDecoder->frame_queue.Push(std::move(f));
            }
        }

        // 变速请求
        if(bSpeedReq.load(std::memory_order_acquire)) {
            // 重建滤镜
            InitAudioFilter(speedTarget.load(std::memory_order_acquire));
            curSpeed = speedTarget.load(std::memory_order_acquire);
            bFirstFilterOutput = true;
            bSpeedReq.store(false, std::memory_order_release);
        }

        // 流控阻塞
        if(audioDecoder->frame_queue.MemSize() >= MAX_AUDIO_FRAME_MEM || audioDecoder->frame_queue.Size() >= MAX_AUDIO_FRAMES_COUNT) {
            std::unique_lock<std::mutex> lock(waitMutex);
            waitCV.wait(lock, [this] {
                return (audioDecoder->frame_queue.MemSize() < MAX_AUDIO_FRAME_MEM && audioDecoder->frame_queue.Size() < MAX_AUDIO_FRAMES_COUNT) ||
                       bSeekReq.load(std::memory_order_acquire) ||
                       bStopAudio.load(std::memory_order_acquire) ||
                       bSpeedReq.load(std::memory_order_acquire);
            });
            if (bStopAudio.load(std::memory_order_acquire)) break;
            continue;
        }

        if(!has_pending) {  // 没有等待重送入的包，将新包送入解码器
            if(!audioDecoder->pkt_queue.Pop(unit, bStopAudio)) break;
            currentSerial = unit.serial;

            // Flush 请求
            if(unit.bFlushReq) {
                avcodec_flush_buffers(audioDecoder->avctxPtr.get());    // 清空解码器内部缓冲
                InitAudioFilter(speedTarget.load());                    // 重建滤镜
                curSpeed = speedTarget.load();
                next_audio_pts = seekTarget.load();                     // 重置PTS推算起点
                bFirstFilterOutput = true;
                continue;
            }

            int ret = avcodec_send_packet(audioDecoder->avctxPtr.get(), unit.pkt.get());
            if(ret == AVERROR(EAGAIN)) { // 解码器内部缓冲满了，标记该包等待重试
                has_pending = true;
                continue;
            }
        }
        else {              // 将等待重送入的包重新送入解码器
            int ret = avcodec_send_packet(audioDecoder->avctxPtr.get(), unit.pkt.get());
            if(ret == AVERROR(EAGAIN)) continue;
            has_pending = false;
        }
    }

    audioDecoder->FreeFilter();
}

void XCMediaCore::VideoDecodeLoop() {
    bool first_frame_after_seek = false;
    PacketUnit unit;
    bool has_pending = false;
    bool bWaitingForKeyFrame = true;
    double next_video_pts = 0.0;
    int framesDroppedAfterSeek = 0;

    int currentSerial = 0;

    while(!bStopVideo.load(std::memory_order_acquire)) {
        // 流控阻塞
        if(videoDecoder->frame_queue.MemSize() >= MAX_VIDEO_FRAME_MEM || videoDecoder->frame_queue.Size() >= MAX_VIDEO_FRAMES_COUNT) {
            std::unique_lock<std::mutex> lock(waitMutex);
            waitCV.wait(lock, [this] {
                return (videoDecoder->frame_queue.MemSize() < MAX_VIDEO_FRAME_MEM && videoDecoder->frame_queue.Size() < MAX_VIDEO_FRAMES_COUNT) ||
                       bSeekReq.load(std::memory_order_acquire) ||
                       bStopVideo.load(std::memory_order_acquire);
            });
            if(bStopVideo.load(std::memory_order_acquire)) break;
            continue;
        }

        while(!bStopVideo.load(std::memory_order_acquire)) {
            // 接收解码后视频帧
            AVFramePtr frame(av_frame_alloc());
            int ret = avcodec_receive_frame(videoDecoder->avctxPtr.get(), frame.get());
            if(ret == AVERROR(EAGAIN)) break;
            if(ret == AVERROR_EOF) {
                videoDecoder->bFinished = true;
                break;
            }
            if (ret < 0) {
                break;
            }

            // 损坏帧丢弃
            if(frame->flags & AV_FRAME_FLAG_CORRUPT) {
                continue;
            }

            // Seek 后等待关键帧
            if(bWaitingForKeyFrame) {
                if((frame->flags & AV_FRAME_FLAG_KEY) || frame->pict_type == AV_PICTURE_TYPE_I || framesDroppedAfterSeek >= 60) {
                    bWaitingForKeyFrame = false;
                    framesDroppedAfterSeek = 0;
                } else { // 丢弃
                    framesDroppedAfterSeek++;
                    continue;
                }
            }

            // 计算 PTS
            double pts = 0.0;
            if(frame->best_effort_timestamp != AV_NOPTS_VALUE) {
                pts = frame->best_effort_timestamp * av_q2d(videoDecoder->time_base)- startTime;
            } else if(frame->pts != AV_NOPTS_VALUE) {
                pts = frame->pts * av_q2d(videoDecoder->time_base) - startTime;
            } else if(frame->pkt_dts != AV_NOPTS_VALUE) {
                pts = frame->pkt_dts * av_q2d(videoDecoder->time_base) - startTime;
            } else {
                pts = next_video_pts;
            }

            // 计算视频帧持续时间
            double frame_duration = 0.0;
            if(frame->duration > 0) {
                frame_duration = frame->duration * av_q2d(videoDecoder->time_base);
            } else if(videoDecoder->avctxPtr->framerate.num > 0 && videoDecoder->avctxPtr->framerate.den > 0) {
                frame_duration = av_q2d(av_inv_q(videoDecoder->avctxPtr->framerate));
            } else {
                frame_duration = 1.0 / 24.0;
            }
            next_video_pts = pts + frame_duration;

            // 丢弃过时帧
            double target = seekTarget.load(std::memory_order_acquire);
            if(first_frame_after_seek && pts < (target - 0.05)) continue;

            FrameUnit f;
            f.pts = pts;
            f.duration = av_q2d(videoDecoder->avctxPtr->time_base);
            if(frame->duration > 0) f.duration = frame->duration * av_q2d(videoDecoder->time_base);
            if(frame->format == AV_PIX_FMT_D3D11) {
                f.data_size = frame->width * frame->height * 3 / 2;
            } else {
                f.data_size = av_image_get_buffer_size((AVPixelFormat)frame->format, frame->width, frame->height, 1);
            }

            f.frame = std::move(frame);
            f.serial = currentSerial;
            // 如果是 Seek 后首帧标记为强制显示
            if(first_frame_after_seek) {
                f.auto_step = true;
                first_frame_after_seek = false;
            }
            videoDecoder->frame_queue.Push(std::move(f));
        }

        if(!has_pending) {
            if(!videoDecoder->pkt_queue.Pop(unit, bStopVideo)) break;

            currentSerial = unit.serial;

            // Flush 请求
            if(unit.bFlushReq) {
                avcodec_flush_buffers(videoDecoder->avctxPtr.get());
                first_frame_after_seek = true;      // 标记下轮循环接收帧是 Seek 后首帧
                bWaitingForKeyFrame = true;         // 标记下轮循环等待关键帧
                framesDroppedAfterSeek = 0;
                next_video_pts = seekTarget.load();

                continue;
            }

            int ret = avcodec_send_packet(videoDecoder->avctxPtr.get(), unit.pkt.get());
            if(ret == AVERROR(EAGAIN)) {
                has_pending = true;
                continue;
            }
        }
        else {
            int ret = avcodec_send_packet(videoDecoder->avctxPtr.get(), unit.pkt.get());
            if(ret == AVERROR(EAGAIN)) continue;
            has_pending = false;
        }
    }
}

std::string FormatAssText(const std::string& assText) {
    if(assText.empty()) return "";

    std::string result = assText;

    // 移除 "Dialogue:" 或 "Comment:" 前缀
    size_t prefixPos = result.find(":");
    if(prefixPos != std::string::npos && prefixPos < 10) {
        result.erase(0, prefixPos + 1);
        while(!result.empty() && result[0] == ' ') result.erase(0, 1);
    }

    // 跳过前面 8 个逗号
    size_t commaCount = 0;
    size_t textStart = 0;
    for(size_t i = 0; i < result.length(); ++i) {
        if(result[i] == ',') {
            commaCount++;
            if(commaCount == 8) {
                textStart = i + 1;
                break;
            }
        }
    }

    if(commaCount >= 8) {
        result = result.substr(textStart);
    }

    // 删除 ASS 特效标签
    static const std::regex tagRegex("\\{[^}]*\\}");
    result = std::regex_replace(result, tagRegex, "");

    // 替换换行符 \N 或 \n 为真实换行
    static const std::regex nlRegex("\\\\[Nn]");
    result = std::regex_replace(result, nlRegex, "\n");

    // 修剪末尾多余的空格或换行
    size_t last = result.find_last_not_of(" \r\n\t");
    if(last != std::string::npos) {
        result = result.substr(0, last + 1);
    } else {
        result = "";
    }

    return result;
}

void XCMediaCore::SubtitleDecodeLoop() {
    PacketUnit unit;
    int currentSerial = 0;

    while(!bStopSubtitle.load(std::memory_order_acquire)) {

        if(!subtitleDecoder->pkt_queue.Pop(unit, bStopSubtitle)) break;

        if(unit.serial != globalSerial.load(std::memory_order_acquire)) continue;

        currentSerial = unit.serial;

        if(unit.bFlushReq) {
            avcodec_flush_buffers(subtitleDecoder->avctxPtr.get());
            continue;
        }

        if (!unit.pkt) continue;

        AVSubtitle sub;
        int got_sub = 0;
        int ret = avcodec_decode_subtitle2(subtitleDecoder->avctxPtr.get(), &sub, &got_sub, unit.pkt.get());

        if(ret >= 0 && got_sub) {
            std::string finalCombinedText = "";
            for(unsigned i = 0; i < sub.num_rects; i++) {
                if(sub.rects[i]->type == SUBTITLE_ASS && sub.rects[i]->ass) {
                    std::string cleaned = FormatAssText(sub.rects[i]->ass);
                    if (!cleaned.empty()) finalCombinedText += cleaned + "\n";
                }
                else if(sub.rects[i]->type == SUBTITLE_TEXT && sub.rects[i]->text) {
                    finalCombinedText += std::string(sub.rects[i]->text) + "\n";
                }
            }

            // 计算 Pts
            double pts = 0.0;
            if(sub.pts != AV_NOPTS_VALUE) {
                pts = sub.pts / (double)AV_TIME_BASE - startTime;
            } else if(unit.pkt->pts != AV_NOPTS_VALUE) {
                pts = unit.pkt->pts * av_q2d(subtitleDecoder->time_base) - startTime;
            }
            if (sub.start_display_time > 0) {
                pts += sub.start_display_time / 1000.0;
            }

            // 字幕持续时间
            double duration = 0.0;
            if(sub.end_display_time > 0 && sub.end_display_time > sub.start_display_time) {
                duration = (sub.end_display_time - sub.start_display_time) / 1000.0;
            } else if (unit.pkt->duration > 0) {
                duration = unit.pkt->duration * av_q2d(subtitleDecoder->time_base);
            } else {
                duration = 3.0;
            }

            avsubtitle_free(&sub);

            if(!finalCombinedText.empty()) {
                FrameUnit f;
                f.pts = pts;
                f.duration = duration;
                f.sub_text = finalCombinedText;
                f.serial = currentSerial;
                subtitleDecoder->frame_queue.Push(std::move(f));
            }
        }
    }
}

void XCMediaCore::VideoRenderLoop() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    timeBeginPeriod(1);

    double frame_timer = av_gettime_relative() / 1000000.0;
    double last_pts = 0.0;

    // 缓冲字幕状态
    std::string currSubtitleText = "";
    double currSubtitleEndTime = -1.0;
    FrameUnit nextSubtitle;
    bool hasNextSubtitle = false;

    while (!bStopVideo.load(std::memory_order_acquire)) {
        if(syncMode == VideoSync && videoDecoder->bFinished.load() && videoDecoder->frame_queue.Size() == 0) {
            if(onPlayCompleted) onPlayCompleted();
            break;
        }

        FrameUnit f;
        if(!videoDecoder->frame_queue.Pop(f, bStopVideo)) break;
        waitCV.notify_all();

        // Seek 后清空字幕缓存
        if(f.auto_step) {
            frame_timer = av_gettime_relative() / 1000000.0;
            last_pts = f.pts;
            currSubtitleText = "";
            currSubtitleEndTime = -1.0;
            hasNextSubtitle = false;
        }

        // 暂停等待（Seek 后第一帧强制显示）
        if(bPauseReq.load(std::memory_order_acquire) && !f.auto_step) {
            std::unique_lock<std::mutex> lock(waitMutex);
            waitCV.wait(lock, [this, &f] {
                return !bPauseReq.load(std::memory_order_acquire) ||
                       bStepReq.load(std::memory_order_acquire) ||
                       bStopVideo.load(std::memory_order_acquire) ||
                       f.serial != globalSerial.load(std::memory_order_acquire);
            });
            frame_timer = av_gettime_relative() / 1000000.0;
            if(bStepReq) { bStepReq = false; }
            if(bStopVideo) break;
        }

        // 计算帧间隔
        double curSpeed = GetSpeed();
        double last_duration = f.pts - last_pts;
        if (std::isnan(last_duration) || last_duration <= 0 || last_duration > 10.0) {
            last_duration = (f.duration > 0 ? f.duration : 0.04);
        }
        last_duration /= curSpeed;

        // 计算延时
        double delay = ComputeTargetDelay(last_duration);

        // 休眠等待该帧的真正显示时刻
        double cur_time = av_gettime_relative() / 1000000.0;
        if(frame_timer <= 0.0) frame_timer = cur_time;
        if(cur_time < frame_timer + delay) {
            double remaining = (frame_timer + delay) - cur_time;
            std::unique_lock<std::mutex> lock(waitMutex);
            waitCV.wait_for(lock, std::chrono::milliseconds((int)(remaining * 1000)), [this, &f]{
                return bStopVideo.load(std::memory_order_acquire) ||
                       f.serial != globalSerial.load(std::memory_order_acquire);
            });
            cur_time = av_gettime_relative() / 1000000.0;
        }

        if(f.serial != globalSerial.load(std::memory_order_acquire)) {
            continue;
        }

        // 更新视频时钟
        videoClock.Set(f.pts, f.serial);

        // 偏差超过 100ms，直接拉回
        frame_timer += delay;
        if(delay > 0 && cur_time - frame_timer > 0.1) {
            frame_timer = cur_time;
        }

        // 过时帧丢弃（确保队列中还有帧）
        double expected_f_duration = (f.duration > 0 ? f.duration : 0.04) / curSpeed;
        if(!f.auto_step && cur_time > frame_timer + expected_f_duration && videoDecoder->frame_queue.Size() > 0) {
            last_pts = f.pts;
            continue;
        }

        // 字幕同步
        if (!currSubtitleText.empty() && f.pts > currSubtitleEndTime) currSubtitleText = "";
        while(true) {
            if (!hasNextSubtitle) hasNextSubtitle = subtitleDecoder->frame_queue.TryPop(nextSubtitle);
            if (hasNextSubtitle) {
                if(nextSubtitle.serial != globalSerial.load(std::memory_order_acquire)) {
                    hasNextSubtitle = false;
                    continue;
                }
                if (f.pts > nextSubtitle.pts + nextSubtitle.duration) { hasNextSubtitle = false; continue; }
                else break;

            } else break;
        }
        if(hasNextSubtitle && f.pts >= nextSubtitle.pts) {
            currSubtitleText = nextSubtitle.sub_text;
            currSubtitleEndTime = nextSubtitle.pts + nextSubtitle.duration;
            hasNextSubtitle = false;
        }

        // 渲染帧
        if(videoRenderer) {
            videoRenderer->PrepareFrame(f.frame.get(), currSubtitleText);
        }

        last_pts = f.pts;
    }

    timeEndPeriod(1);
}

bool XCMediaCore::InitAudioFilter(float initSpeed) {
    audioDecoder->FreeFilter();

    AVFilterGraph* graph = nullptr;
    AVFilterContext* src_ctx = nullptr;
    AVFilterContext* sink_ctx = nullptr;
    AVFilterContext* atempo_ctx = nullptr;
    std::string speed_str;

    graph = avfilter_graph_alloc();
    if(!graph) return false;

    // 准备 abuffer 的参数
    const AVCodecContext* codec_ctx = audioDecoder->avctxPtr.get();
    char args[512];

    AVChannelLayout ch_layout = codec_ctx->ch_layout;
    if (ch_layout.nb_channels <= 0)
        av_channel_layout_default(&ch_layout, 2);

    char layout_buf[64];
    av_channel_layout_describe(&ch_layout, layout_buf, sizeof(layout_buf));

    snprintf(args, sizeof(args),
             "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
             audioDecoder->time_base.num, audioDecoder->time_base.den,
             codec_ctx->sample_rate,
             av_get_sample_fmt_name(codec_ctx->sample_fmt),
             layout_buf);

    // 获取 filter
    const AVFilter* buffer_src = avfilter_get_by_name("abuffer");
    const AVFilter* buffer_sink = avfilter_get_by_name("abuffersink");
    const AVFilter* atempo = avfilter_get_by_name("atempo");

    if(!buffer_src || !buffer_sink || !atempo) goto fail;

    // 创建并连接 filter
    if(avfilter_graph_create_filter(&src_ctx, buffer_src, "in", args, nullptr, graph) < 0)
        goto fail;

    if(avfilter_graph_create_filter(&sink_ctx, buffer_sink, "out", nullptr, nullptr, graph) < 0)
        goto fail;

    speed_str = std::to_string(initSpeed);
    if(avfilter_graph_create_filter(&atempo_ctx, atempo, "speed_filter", speed_str.c_str(), nullptr, graph) < 0)
        goto fail;

    // 连接 filter 链
    if (avfilter_link(src_ctx, 0, atempo_ctx, 0) < 0) goto fail;
    if (avfilter_link(atempo_ctx, 0, sink_ctx, 0) < 0) goto fail;

    // 配置 graph
    if(avfilter_graph_config(graph, nullptr) < 0) goto fail;

    audioDecoder->filter_graph = graph;
    audioDecoder->filter_src_ctx = src_ctx;
    audioDecoder->filter_sink_ctx = sink_ctx;

    return true;

fail:
    if(graph) avfilter_graph_free(&graph);
    return false;
}

bool XCMediaCore::HasEnoughPackets() {
    if(bStopDemux.load(std::memory_order_acquire)) return true;

    bool audioFull = audioStreamIndex.load() < 0 ||
                     audioDecoder->pkt_queue.MemSize() >= MAX_PACKET_MEM ||
                     audioDecoder->pkt_queue.Size() >= MAX_PACKET_COUNT;

    bool videoFull = videoStreamIndex.load() < 0 ||
                     videoDecoder->pkt_queue.MemSize() >= MAX_PACKET_MEM ||
                     videoDecoder->pkt_queue.Size() >= MAX_PACKET_COUNT;

    return audioFull && videoFull;
}

double XCMediaCore::ComputeTargetDelay(double delay)
{
    double sync_threshold, diff = 0;

    if (syncMode != VideoSync) {
        diff = videoClock.Get() - GetMasterClock();

        sync_threshold = std::max(0.04, std::min(0.1, delay));

        // 只对10秒偏差的做平滑同步
        if(!std::isnan(diff) && std::fabs(diff) < 10.0) {
            if(diff <= -sync_threshold) {
                // 视频落后，减小 delay，视频加速播放
                delay = std::max(0.0, delay + diff);
            } else if(diff >= sync_threshold && delay > 0.04) {
                // 视频超前，增加 delay，视频放慢等待
                delay = delay + diff;
            } else if (diff >= sync_threshold) {
                // 视频超前，且原本的 delay 很小，直接翻倍等待
                delay = 2 * delay;
            }
        }
    }
    return delay;
}

bool XCMediaCore::IsLiveStream(AVFormatContext *ic)
{
    if(!ic) return false;

    if(!strcmp(ic->iformat->name, "rtp") || !strcmp(ic->iformat->name, "rtsp") || !strcmp(ic->iformat->name, "sdp"))
        return true;

    if(ic->pb && (!strncmp(ic->url, "rtp:", 4) || !strncmp(ic->url, "udp:", 4)))
        return true;

    if(ic->duration == AV_NOPTS_VALUE || ic->duration <= 0)
        return true;

    return false;
}

void XCMediaCore::SetD3D11Device(void* device) {
    if(!device) return;

    if(hwdeviceCtx) {
        AVHWDeviceContext* ctx = (AVHWDeviceContext*)hwdeviceCtx->data;
        AVD3D11VADeviceContext* hwctx = (AVD3D11VADeviceContext*)ctx->hwctx;
        if (hwctx->device == device) return;
        av_buffer_unref(&hwdeviceCtx);
    }

    ID3D11Device* dev = static_cast<ID3D11Device*>(device);

    // 开启多线程保护
    ID3D10Multithread* pMultithread = nullptr;
    if(SUCCEEDED(dev->QueryInterface(__uuidof(ID3D10Multithread), (void**)&pMultithread)) && pMultithread) {
        pMultithread->SetMultithreadProtected(TRUE);
        pMultithread->Release();
    }

    AVBufferRef* new_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if(!new_ctx) return;

    AVHWDeviceContext* device_ctx = (AVHWDeviceContext*)new_ctx->data;
    AVD3D11VADeviceContext* d3d11_hwctx = (AVD3D11VADeviceContext*)device_ctx->hwctx;

    dev->AddRef();
    d3d11_hwctx->device = dev;

    ID3D11DeviceContext* dev_ctx = nullptr;
    dev->GetImmediateContext(&dev_ctx);
    d3d11_hwctx->device_context = dev_ctx;

    if(av_hwdevice_ctx_init(new_ctx) < 0) {
        av_buffer_unref(&new_ctx);
        return;
    }

    hwdeviceCtx = new_ctx;
}
