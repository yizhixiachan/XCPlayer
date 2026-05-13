#ifndef XCAUDIOENGINE_H
#define XCAUDIOENGINE_H

#include "XCType.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>
#include <functional>
#include <thread>

using Microsoft::WRL::ComPtr;

class SwrContext;

class XCAudioEngine : public IMMNotificationClient {
public:
    explicit XCAudioEngine();
    ~XCAudioEngine();

    // 引擎状态控制
    bool Start(Decoder* dec, Clock* clk);
    void Abort();
    void Stop();
    void SetPause(bool pause);
    void Flush();

    // 音频设置
    void SetVolume(float v);
    float GetVolume() const { return targetVolume.load(std::memory_order_acquire); }
    void SetMute(bool mute);
    bool IsMute() const { return bMute.load(std::memory_order_acquire); }
    void SetExclusiveMode(bool exclusive);
    void SetReplayGainEnabled(bool enable);

    void SetPlayEndCallback(std::function<void()> cb) { onPlayEnd = cb; }
    void SetConsumeCallback(std::function<void()> cb) { onConsumeCallback = cb; }

protected:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override { return 1; }
    STDMETHODIMP_(ULONG) Release() override { return 1; }
    STDMETHODIMP OnDefaultDeviceChanged(EDataFlow, ERole, LPCWSTR) override;
    STDMETHODIMP OnDeviceAdded(LPCWSTR) override { return S_OK; }
    STDMETHODIMP OnDeviceRemoved(LPCWSTR) override { return S_OK; }
    STDMETHODIMP OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
    STDMETHODIMP OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

private:
    bool InitWASAPI(bool isExclusive);
    void DestroyWASAPI();
    void RenderLoop();

    void ProcessAudio();
    bool FetchAndResampleFrame(int& outSerial);
    void ApplyVolumeAndGain(int total_samples);
    void CalculateAudioClock(UINT32 bufSize, int serial);

private:
    std::function<void()> onPlayEnd{};
    std::function<void()> onConsumeCallback{};
    Decoder* decoder{nullptr};
    Clock* audioClock{nullptr};

    // 线程控制与状态标记
    std::thread renderThread;
    std::atomic<bool> bAbortReq{false};
    std::atomic<bool> bPauseReq{false};
    std::atomic<bool> bFlushReq{false};
    std::atomic<bool> bDeviceChanged{false};
    std::atomic<bool> bSharedModeReq{false};
    std::atomic<bool> bExclusiveModeReq{false};

    std::atomic<float> targetVolume{0.25f};
    std::atomic<bool> bMute{false};
    std::atomic<bool> bReplayGainEnabled{true};

    bool bIsPlaying = false;
    bool bExclusiveMode = false;
    bool bLastBufferSubmitted = false;
    bool bEndNotified = false;

    // WASAPI 核心对象
    ComPtr<IAudioClient> pAudioClient;
    ComPtr<IAudioRenderClient> pRenderClient;
    ComPtr<ISimpleAudioVolume> pSimpleVolume;
    HANDLE hEvent{nullptr};
    HANDLE hMmTask{nullptr};
    WAVEFORMATEXTENSIBLE wfx{};

    // FFmpeg 重采样与缓存管理
    SwrContext* swr_ctx{nullptr};
    AVChannelLayout m_cached_ch_layout{};
    int cached_sample_rate{0};
    int cached_format{1};

    std::vector<uint8_t> audio_buf;
    size_t audio_buf_index{0};
    size_t audio_buf_size{0};

    // 时钟与增益控制
    double audio_clock_pts{0};
    double current_frame_speed{1.0};
    float replayGainMultiplier{1.0f};
    bool bReplayGainParsed = false;
};

#endif
