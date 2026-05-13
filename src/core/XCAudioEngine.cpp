#include "XCAudioEngine.h"
#include <avrt.h>
extern "C"
{
#include <libswresample/swresample.h>
#include <libavutil/replaygain.h>
}

XCAudioEngine::XCAudioEngine() {}

XCAudioEngine::~XCAudioEngine() { Stop(); }

bool XCAudioEngine::Start(Decoder* dec, Clock* clk) {
    Stop();
    decoder = dec;
    audioClock = clk;

    bAbortReq = false;
    bPauseReq = false;
    bFlushReq = false;
    bDeviceChanged = false;
    bSharedModeReq = false;
    bExclusiveModeReq = false;

    renderThread = std::thread(&XCAudioEngine::RenderLoop, this);
    return true;
}

void XCAudioEngine::Abort()
{
    bAbortReq = true;
    if(hEvent) SetEvent(hEvent);
}

void XCAudioEngine::Stop() {
    Abort();

    if(renderThread.joinable()) {
        if(std::this_thread::get_id() != renderThread.get_id()) {
            renderThread.join();
        } else {
            renderThread.detach();
        }
    }
}

void XCAudioEngine::SetPause(bool pause)
{
    if(bPauseReq.exchange(pause, std::memory_order_acq_rel) == pause) return;
    if(hEvent) SetEvent(hEvent);
}

void XCAudioEngine::Flush()
{
    bFlushReq.store(true, std::memory_order_release);
    if(hEvent) SetEvent(hEvent);
}

void XCAudioEngine::SetVolume(float v)
{
    targetVolume.store(std::clamp(v, 0.0f, 1.0f), std::memory_order_release);
}

void XCAudioEngine::SetMute(bool mute)
{
    if(bMute.exchange(mute, std::memory_order_acq_rel) == mute) return;
    if(!bExclusiveMode && pSimpleVolume) pSimpleVolume->SetMute(mute ? 1 : 0, nullptr);
}

void XCAudioEngine::SetExclusiveMode(bool exclusive)
{
    if(bExclusiveMode != exclusive) {
        exclusive ? bExclusiveModeReq.store(true, std::memory_order_release)
                  : bSharedModeReq.store(true, std::memory_order_release);
        if(hEvent) SetEvent(hEvent);
    }
}

void XCAudioEngine::SetReplayGainEnabled(bool enable)
{
    bReplayGainEnabled.store(enable, std::memory_order_release);
}

STDMETHODIMP XCAudioEngine::QueryInterface(REFIID riid, void** ppv) {
    if(ppv == nullptr) return E_POINTER;

    if(riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
        *ppv = static_cast<IMMNotificationClient*>(this);
        return S_OK;
    }

    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP XCAudioEngine::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) {
    // 发出设备变更信号
    if(flow == eRender && role == eMultimedia) bDeviceChanged.store(true, std::memory_order_release);
    return S_OK;
}

bool XCAudioEngine::InitWASAPI(bool isExclusive) {
    ComPtr<IMMDeviceEnumerator> pEnum;
    ComPtr<IMMDevice> pDev;

    if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pEnum)))) return false;
    // 注册设备变更通知
    pEnum->RegisterEndpointNotificationCallback(this);

    if(FAILED(pEnum->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDev))) return false;
    if(FAILED(pDev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient))) return false;

    // 获取系统默认混音格式 （共享模式）
    WAVEFORMATEX* pwfx = nullptr;
    pAudioClient->GetMixFormat(&pwfx);
    WAVEFORMATEXTENSIBLE originalWfx = *reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pwfx);
    CoTaskMemFree(pwfx);

    HRESULT hr = E_FAIL;

    // 独占模式：尝试匹配原始采样率
    if(isExclusive) {
        // 获取原始采样率
        uint32_t targetRate = originalWfx.Format.nSamplesPerSec;
        int targetChannels = originalWfx.Format.nChannels;
        if(decoder && decoder->avctxPtr) {
            auto ctx = decoder->avctxPtr.get();
            if(ctx->sample_rate > 0) targetRate = ctx->sample_rate;
            if(ctx->ch_layout.nb_channels > 0) targetChannels = ctx->ch_layout.nb_channels;
        }

        // 初始化采样率尝试列表
        std::vector<uint32_t> sampleRates = { targetRate, 48000, 44100 };
        std::vector<int> bitDepths = { 24, 32, 16 };

        REFERENCE_TIME defaultPeriod, minPeriod;
        pAudioClient->GetDevicePeriod(&defaultPeriod, &minPeriod);

        bool exclusiveSuccess = false;

        // 遍历采样率和位深度组合，直到成功初始化
        for(uint32_t rate : sampleRates) {
            for(int bits : bitDepths) {
                WAVEFORMATEXTENSIBLE tryWfx = {};
                tryWfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
                tryWfx.Format.nChannels = targetChannels;
                tryWfx.Format.nSamplesPerSec = rate;
                tryWfx.Format.wBitsPerSample = (bits == 24) ? 32 : bits;
                tryWfx.Samples.wValidBitsPerSample = bits;
                tryWfx.Format.nBlockAlign = (tryWfx.Format.wBitsPerSample / 8) * targetChannels;
                tryWfx.Format.nAvgBytesPerSec = rate * tryWfx.Format.nBlockAlign;
                tryWfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
                tryWfx.dwChannelMask = (targetChannels == 2) ? (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT) : SPEAKER_FRONT_CENTER;
                tryWfx.SubFormat = (bits == 32) ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;

                // 独占模式初始化
                hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                              minPeriod, minPeriod, &tryWfx.Format, nullptr);

                if(hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
                    UINT32 frames = 0;
                    pAudioClient->GetBufferSize(&frames);
                    REFERENCE_TIME alignedPeriod = (REFERENCE_TIME)((10000.0 * 1000 / tryWfx.Format.nSamplesPerSec * frames) + 0.5);
                    pAudioClient.Reset();
                    pDev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
                    // 用对齐后的周期重新初始化
                    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                                  alignedPeriod, alignedPeriod, &tryWfx.Format, nullptr);
                }

                if(SUCCEEDED(hr)) {
                    wfx = tryWfx;
                    exclusiveSuccess = true;
                    break;
                }

                // 初始化失败，重置 IAudioClient
                pAudioClient.Reset();
                pDev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
            }

            if(exclusiveSuccess) break;
        }

        // 所有组合都失败，回退为共享模式
        if(!exclusiveSuccess) isExclusive = false;
    }
    // 共享模式：使用系统混音格式
    else {
        hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                      1000000, 0, &originalWfx.Format, nullptr);
        if(SUCCEEDED(hr)) wfx = originalWfx;
        else return false;
    }

    bExclusiveMode = isExclusive;

    // 设置事件驱动
    hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    pAudioClient->SetEventHandle(hEvent);
    pAudioClient->GetService(IID_PPV_ARGS(&pRenderClient));

    if(!bExclusiveMode && SUCCEEDED(pAudioClient->GetService(IID_PPV_ARGS(&pSimpleVolume)))) {
        pSimpleVolume->SetMute(bMute.load(std::memory_order_acquire), nullptr);
        pSimpleVolume->SetMasterVolume(targetVolume.load(std::memory_order_acquire), nullptr);
    }

    return true;
}

void XCAudioEngine::DestroyWASAPI() {
    if(pAudioClient) {
        pAudioClient->Stop();
        pAudioClient->Reset();
    }
    pRenderClient.Reset();
    pSimpleVolume.Reset();
    pAudioClient.Reset();

    if(hEvent) {
        CloseHandle(hEvent);
        hEvent = nullptr;
    }

    av_channel_layout_uninit(&m_cached_ch_layout);
    if(swr_ctx) {
        swr_free(&swr_ctx);
        swr_ctx = nullptr;
    }

    bIsPlaying = false;
}

void XCAudioEngine::RenderLoop() {
    // 初始化 COM 为多线程模式
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // 将当前线程注册为 "Pro Audio"，设置为最高线程优先级
    DWORD taskIndex = 0;
    hMmTask = AvSetMmThreadCharacteristicsA("Pro Audio", &taskIndex);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    // 内部状态初始化
    audio_buf_index = 0; audio_buf_size = 0; audio_clock_pts = 0;
    bEndNotified = false; bIsPlaying = false; bLastBufferSubmitted = false;
    bReplayGainParsed = false; replayGainMultiplier = 1.0f;

    if(!InitWASAPI(bExclusiveMode)) {
        bAbortReq.store(true, std::memory_order_release);
    }

    while(!bAbortReq.load(std::memory_order_acquire)) {
        // 设备切换、共享模式切换
        if(bDeviceChanged.load(std::memory_order_acquire) ||
            bSharedModeReq.load(std::memory_order_acquire) ||
            bExclusiveModeReq.load(std::memory_order_acquire)) {
            bool isExclusive = bExclusiveModeReq.load(std::memory_order_acquire);
            bDeviceChanged.store(false, std::memory_order_release);
            bSharedModeReq.store(false, std::memory_order_release);
            bExclusiveModeReq.store(false, std::memory_order_release);

            // 销毁旧设备，重新初始化
            DestroyWASAPI();
            audio_buf_size = 0;
            audio_buf_index = 0;
            audio_buf.clear();

            if(!InitWASAPI(isExclusive)) break;

            bIsPlaying = false;
        }

        // Flush 请求
        if(bFlushReq.load(std::memory_order_acquire)) {
            bFlushReq.store(false, std::memory_order_release);
            pAudioClient->Stop();
            pAudioClient->Reset();
            bIsPlaying = false;
            audio_buf_size = 0;
            audio_buf_index = 0;
            bEndNotified = false;
            bLastBufferSubmitted = false;

            if(audioClock) audio_clock_pts = audioClock->Get();
            else audio_clock_pts = 0;
        }

        // 暂停
        if(bPauseReq.load(std::memory_order_acquire)) {
            if(bIsPlaying) {
                pAudioClient->Stop();
                bIsPlaying = false;
            }

            WaitForSingleObject(hEvent, 50);
            continue;
        }

        // 启动前预填充满缓冲区
        if(!bIsPlaying && !bEndNotified) {
            ProcessAudio();
            if(SUCCEEDED(pAudioClient->Start())) bIsPlaying = true;
        }

        // 等待缓冲区需要填充信号
        DWORD waitRes = WaitForSingleObject(hEvent, 100);

        if(bAbortReq.load(std::memory_order_acquire)) break;

        if(!bExclusiveMode && pSimpleVolume) {
            pSimpleVolume->SetMasterVolume(targetVolume.load(std::memory_order_acquire), nullptr);
        }

        // 收到信号
        if(waitRes == WAIT_OBJECT_0) {
            ProcessAudio();
        }

        // 播放结束判定
        if(decoder && decoder->bFinished.load(std::memory_order_acquire) &&
            decoder->frame_queue.Size() == 0 && audio_buf_index >= audio_buf_size) {

            if(bLastBufferSubmitted && !bEndNotified) {
                bEndNotified = true;
                bIsPlaying = false;
                pAudioClient->Stop();
                if(onPlayEnd) onPlayEnd();
            }
        }
    }

    DestroyWASAPI();
    if(hMmTask) AvRevertMmThreadCharacteristics(hMmTask);
    CoUninitialize();
}

void XCAudioEngine::ProcessAudio() {
    if(!pAudioClient || !pRenderClient) return;

    // 获取 WASAPI 缓冲区大小
    UINT32 bufSize = 0;
    if(FAILED(pAudioClient->GetBufferSize(&bufSize))) return;

     // 计算可填充的帧数
    UINT32 framesAvailable = bufSize;
    if(!bExclusiveMode) {
        // 共享模式：需要减去当前未被播放的 padding
        UINT32 padding = 0;
        if(SUCCEEDED(pAudioClient->GetCurrentPadding(&padding))) {
            framesAvailable = bufSize - padding;
        }
    }   // 独占模式：整个缓冲区都是我们的

    if(framesAvailable == 0) return;

    // 获取 WASAPI 缓冲区的写指针
    BYTE* pData = nullptr;
    if(FAILED(pRenderClient->GetBuffer(framesAvailable, &pData))) return;

    const int bytesPerFrame = wfx.Format.nBlockAlign;   // 每帧字节数
    UINT32 framesWritten = 0;                           // 已写入帧数
    bool bNoRealDataWritten = true;                     // 是否未写入任何真实数据

    int serial = 0;

    // 从音频帧队列获取数据并写入 WASAPI 缓冲区
    while(framesWritten < framesAvailable) {
        // 如果内部缓冲区用尽，则提取下一帧
        if(audio_buf_index >= audio_buf_size) {
            if(!FetchAndResampleFrame(serial)) {
                break; // 取不到数据，跳出循环用静音填充
            }
        }

        // 计算本次可拷贝的帧数
        UINT32 canCopy = std::min<UINT32>((UINT32)(
            audio_buf_size - audio_buf_index) / bytesPerFrame,  // 内部缓冲区剩余
            framesAvailable - framesWritten                     // WASAPI 缓冲区剩余
        );

        if(canCopy > 0) {
            bNoRealDataWritten = false;

            // 拷贝数据：内部缓冲区 → WASAPI 缓冲区
            memcpy(pData + (framesWritten * bytesPerFrame), audio_buf.data() + audio_buf_index, canCopy * bytesPerFrame);

            framesWritten += canCopy;
            audio_buf_index += (canCopy * bytesPerFrame);

            // 时钟步进：每写入一个采样，时钟就增加对应的时长
            audio_clock_pts += ((double)canCopy / wfx.Format.nSamplesPerSec) * current_frame_speed;
        } else {
            break;
        }
    }

    // 数据不足静音填充
    if(framesWritten < framesAvailable) {
        // 剩下的空间填 0
        memset(pData + (framesWritten * bytesPerFrame), 0, (framesAvailable - framesWritten) * bytesPerFrame);
        framesWritten = framesAvailable;

        // 判断是否播放结束
        if(decoder && decoder->bFinished.load(std::memory_order_acquire) && decoder->frame_queue.Size() == 0 && audio_buf_index >= audio_buf_size) {
            if(bNoRealDataWritten) {
                bLastBufferSubmitted = true;
            }
        }
    }

    // 释放 WASAPI 缓冲区，播放数据
    pRenderClient->ReleaseBuffer(framesWritten, 0);

    // 更新音频时钟
    CalculateAudioClock(bufSize, serial);
}

bool XCAudioEngine::FetchAndResampleFrame(int& outSerial)
{
    FrameUnit f;
    // 非阻塞取帧
    if(!decoder->frame_queue.TryPop(f)) return false;

    outSerial = f.serial;

    // 解析 ReplayGain (每首歌第一帧解析一次)
    if(!bReplayGainParsed) {
        AVFrameSideData* rg_sd = av_frame_get_side_data(f.frame.get(), AV_FRAME_DATA_REPLAYGAIN);
        if(rg_sd) {
            AVReplayGain* rg = (AVReplayGain*)rg_sd->data;
            bool useTrack = (rg->track_gain != INT32_MIN);
            int32_t gain_val = useTrack ? rg->track_gain : rg->album_gain;
            uint32_t peak_val = useTrack ? rg->track_peak : rg->album_peak;

            if(gain_val != INT32_MIN) {
                // 计算分贝增益
                float gain_db = gain_val / 100000.0f;
                // 计算振幅缩放系数，db = 20 x log10(linear_gain)
                float linear_gain = std::pow(10.0f, gain_db / 20.0f);

                if(peak_val > 0) {
                    float peak = peak_val / 100000.0f;
                    if(peak * linear_gain > 1.0f) linear_gain = 1.0f / peak;
                }
                replayGainMultiplier = linear_gain;
            }
        }

        // 标记为已解析
        bReplayGainParsed = true;
    }

    current_frame_speed = f.speed;

    // 一帧音频被消耗，通知音频解码线程别睡了，起来解码
    if(onConsumeCallback) onConsumeCallback();


    // 检测音频格式变更并重建重采样器
    bool bFormatChanged = false;
    if(swr_ctx) {
        if(cached_sample_rate != f.frame->sample_rate ||
            cached_format != f.frame->format ||
            av_channel_layout_compare(&m_cached_ch_layout, &f.frame->ch_layout) != 0) {
            bFormatChanged = true;
        }
    }

    if(!swr_ctx || bFormatChanged) {
        if(swr_ctx) swr_free(&swr_ctx);

        AVChannelLayout out_ch;
        av_channel_layout_default(&out_ch, wfx.Format.nChannels);
        AVSampleFormat out_fmt = (wfx.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) ? AV_SAMPLE_FMT_FLT :
                                     (wfx.Format.wBitsPerSample == 32) ? AV_SAMPLE_FMT_S32 : AV_SAMPLE_FMT_S16;

        swr_alloc_set_opts2(&swr_ctx, &out_ch, out_fmt, wfx.Format.nSamplesPerSec,
                            &f.frame->ch_layout, (AVSampleFormat)f.frame->format,
                            f.frame->sample_rate, 0, nullptr);
        swr_init(swr_ctx);
        av_channel_layout_uninit(&out_ch);

        // 缓存当前格式
        cached_sample_rate = f.frame->sample_rate;
        cached_format = f.frame->format;
        av_channel_layout_copy(&m_cached_ch_layout, &f.frame->ch_layout);
    }

    // 补偿重采样器内滞留数据的延迟
    int64_t delay_samples = swr_get_delay(swr_ctx, f.frame->sample_rate);
    double delay_sec = (double)delay_samples / f.frame->sample_rate;
    audio_clock_pts = f.pts - (delay_sec * current_frame_speed);

    // 开始重采样转换
    int out_samples = av_rescale_rnd(f.frame->nb_samples, wfx.Format.nSamplesPerSec, f.frame->sample_rate, AV_ROUND_UP);
    int bytesPerFrame = wfx.Format.nBlockAlign;

    // 预分配内部缓冲区
    audio_buf.resize(out_samples * bytesPerFrame);
    uint8_t* out_ptr = audio_buf.data();
    int converted = swr_convert(swr_ctx, &out_ptr, out_samples, (const uint8_t**)f.frame->data, f.frame->nb_samples);

    if(converted <= 0) {
        audio_buf_size = 0;
        return false;
    }

    // 更新内部缓冲区状态
    audio_buf_size = converted * bytesPerFrame; // 有效数据字节数
    audio_buf_index = 0;                        // 读指针归零


    // 设置音量和 Replay Gain
    ApplyVolumeAndGain(converted * wfx.Format.nChannels);

    return true;
}

void XCAudioEngine::ApplyVolumeAndGain(int total_samples)
{
    float scale = 1.0f;

    // 独占模式需要直接控制音量
    if(bExclusiveMode) {
        float vol = targetVolume.load(std::memory_order_acquire);
        if (bMute.load(std::memory_order_acquire)) vol = 0.0f;
        scale *= vol;
    }

    // 应用 Replay Gain
    if(bReplayGainEnabled.load(std::memory_order_acquire)) {
        scale *= replayGainMultiplier;
    }

    // PCM 缩放
    if(std::abs(scale - 1.0f) > 1e-3f) {
        if(std::abs(scale - 0.0f) < 1e-3f) {
            memset(audio_buf.data(), 0, audio_buf_size);

        } else if(wfx.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            float* pcm = reinterpret_cast<float*>(audio_buf.data());
            for (int i = 0; i < total_samples; ++i) pcm[i] = std::clamp(pcm[i] * scale, -1.0f, 1.0f);

        } else if(wfx.Format.wBitsPerSample == 32) {
            int32_t* pcm = reinterpret_cast<int32_t*>(audio_buf.data());
            for (int i = 0; i < total_samples; ++i) {
                int64_t sample = static_cast<int64_t>(pcm[i] * scale);
                pcm[i] = static_cast<int32_t>(std::clamp(sample, static_cast<int64_t>(INT32_MIN), static_cast<int64_t>(INT32_MAX)));
            }

        } else if(wfx.Format.wBitsPerSample == 16) {
            int16_t* pcm = reinterpret_cast<int16_t*>(audio_buf.data());
            for (int i = 0; i < total_samples; ++i) {
                int32_t sample = static_cast<int32_t>(pcm[i] * scale);
                pcm[i] = static_cast<int16_t>(std::clamp(sample, -32768, 32767));
            }
        }
    }
}

void XCAudioEngine::CalculateAudioClock(UINT32 bufSize, int serial)
{
    if(!audioClock) return;

    // 计算硬件缓冲延迟（声卡里还没播完的数据时长）
    double hardware_delay = 0.0;
    if(bExclusiveMode) {    // 独占模式：整个 bufSize 都是我们的缓冲区
        hardware_delay = (double)bufSize / wfx.Format.nSamplesPerSec;
    } else {                // 共享模式：使用当前未播放的 padding
        UINT32 padding = 0;
        if(SUCCEEDED(pAudioClient->GetCurrentPadding(&padding))) {
            hardware_delay = (double)padding / wfx.Format.nSamplesPerSec;
        }
    }

    // 更新音频时钟 = 已经塞给声卡的数据的尾部时间 - 声卡还没播完的排队数据时长
    audioClock->Set(audio_clock_pts - hardware_delay * current_frame_speed, serial);
}
