#ifndef XCMEDIACORE_H
#define XCMEDIACORE_H

#include <thread>
#include <functional>
#include "XCType.h"

class XCAudioEngine;

class XCMediaCore
{
public:
    explicit XCMediaCore(std::function<void()> onPlayCompleted = nullptr);
    ~XCMediaCore();

    enum SyncMode {
        AudioSync = 0, VideoSync, ExternalSync
    };

    // 播放控制
    int Play(const std::string& url, AVDictionary* opts = nullptr);
    void SetPause(bool pause);
    void Seek(double sec);
    void Abort();
    void Stop();
    void SetSpeed(float multiplier);
    float GetSpeed() const;
    double GetMasterClock() const;

    double GetLatestPacketPts() const { return latestPacketPts.load(std::memory_order_acquire); }
    AVFormatContext* GetFormatContext() const { return inputCtxPtr.get(); }

    // 流切换
    void OpenAudioStream(int index);
    void OpenVideoStream(int index);
    void OpenSubtitleStream(int index);

    // 状态获取
    bool IsIdle() const { return bStopDemux.load(std::memory_order_acquire); }
    bool IsPlaying() const { return !bStopDemux.load(std::memory_order_acquire) && !bPauseReq.load(std::memory_order_acquire); }
    bool IsLive() const { return !bStopDemux.load(std::memory_order_acquire) && isLive; }
    int GetAudioStreamIndex() const { return audioStreamIndex.load(); }
    int GetVideoStreamIndex() const { return videoStreamIndex.load(); }
    int GetSubtitleStreamIndex() const { return subtitleStreamIndex.load(); }

    // 音频设置
    void SetVolume(float value);
    float GetVolume() const;
    void SetMute(bool mute);
    bool IsMute() const;
    void SetExclusiveMode(bool exclusive);
    void SetReplayGainEnabled(bool enable);

    // 视频设置
    void SetHWAccelEnabled(bool enable);
    bool IsHWEnabled() const { return bHWAccel.load(std::memory_order_acquire); }
    void SetVideoRenderer(IVideoRenderer* renderer) { videoRenderer = renderer; }
    void SetD3D11Device(void* device);

private:
    void DemuxLoop();
    void AudioDecodeLoop();
    void VideoDecodeLoop();
    void SubtitleDecodeLoop();
    void VideoRenderLoop();

    bool InitAudioFilter(float initSpeed = 1.0);
    bool HasEnoughPackets();
    double ComputeTargetDelay(double delay);
    static bool IsLiveStream(AVFormatContext* ic);

    std::function<void()> onPlayCompleted;

    // 时钟
    Clock audioClock{};
    Clock videoClock{};
    Clock externalClock{};

    // FFmpeg 核心
    std::unique_ptr<Decoder> audioDecoder;
    std::unique_ptr<Decoder> videoDecoder;
    std::unique_ptr<Decoder> subtitleDecoder;

    AVFormatContextPtr inputCtxPtr;
    std::unique_ptr<XCAudioEngine> audioEngine;
    IVideoRenderer* videoRenderer{nullptr};
    AVBufferRef* hwdeviceCtx{nullptr};

    std::mutex waitMutex;
    std::condition_variable waitCV;

    std::thread demuxThread;
    std::thread audioDecodeThread;
    std::thread videoDecodeThread;
    std::thread subtitleDecodeThread;
    std::thread videoRenderThread;

    std::atomic<double> seekTarget{0.0};
    std::atomic<float> speedTarget{1.0};

    std::atomic<int> audioStreamIndex{-1};
    std::atomic<int> videoStreamIndex{-1};
    std::atomic<int> subtitleStreamIndex{-1};

    std::atomic<int> globalSerial{0};

    std::atomic<double> latestPacketPts{0.0};

    double startTime{0.0};

    SyncMode syncMode{AudioSync};

    std::atomic<bool> bPauseReq{true};
    std::atomic<bool> bSeekReq{false};
    std::atomic<bool> bSpeedReq{false};
    std::atomic<bool> bStepReq{false};
    std::atomic<bool> bHWAccel{true};
    std::atomic<bool> bStopDemux{false};
    std::atomic<bool> bStopAudio{false};
    std::atomic<bool> bStopVideo{false};
    std::atomic<bool> bStopSubtitle{false};
    std::atomic<bool> bHWAccelChanged{false};

    bool isLive{false};


    // 缓冲限制参数
    static constexpr size_t MAX_PACKET_MEM{100 * 1024 * 1024};
    static constexpr int MAX_PACKET_COUNT{500};

    static constexpr size_t MAX_VIDEO_FRAME_MEM = 128 * 1024 * 1024;
    static constexpr size_t MAX_AUDIO_FRAME_MEM = 1 * 1024 * 1024;

    static constexpr int MAX_VIDEO_FRAMES_COUNT = 3;
    static constexpr int MAX_AUDIO_FRAMES_COUNT = 9;
};

#endif
