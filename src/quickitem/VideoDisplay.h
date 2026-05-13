#ifndef VIDEODISPLAY_H
#define VIDEODISPLAY_H

#include <QQuickItem>
#include "core/XCType.h"
#include <d3d11_1.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
class SwsContext;

class VideoDisplay : public QQuickItem, public IVideoRenderer
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString subtitleText READ GetSubtitleText NOTIFY subtitleTextChanged)
    Q_PROPERTY(bool isHDREnabled READ IsHDREnabled NOTIFY hdrEnabledChanged)
    Q_PROPERTY(int videoWidth READ GetVideoWidth NOTIFY videoSizeChanged)
    Q_PROPERTY(int videoHeight READ GetVideoHeight NOTIFY videoSizeChanged)

    Q_PROPERTY(qreal brightness READ GetBrightness WRITE SetBrightness NOTIFY brightnessChanged)
    Q_PROPERTY(qreal contrast READ GetContrast WRITE SetContrast NOTIFY contrastChanged)
    Q_PROPERTY(qreal hue READ GetHue WRITE SetHue NOTIFY hueChanged)
    Q_PROPERTY(qreal saturation READ GetSaturation WRITE SetSaturation NOTIFY saturationChanged)
    Q_PROPERTY(qreal noiseReduction READ GetNoiseReduction WRITE SetNoiseReduction NOTIFY noiseReductionChanged)
    Q_PROPERTY(qreal edgeEnhancement READ GetEdgeEnhancement WRITE SetEdgeEnhancement NOTIFY edgeEnhancementChanged)

    Q_PROPERTY(bool brightnessSupported READ IsBrightnessSupported NOTIFY filtersReady)
    Q_PROPERTY(bool contrastSupported READ IsContrastSupported NOTIFY filtersReady)
    Q_PROPERTY(bool hueSupported READ IsHueSupported NOTIFY filtersReady)
    Q_PROPERTY(bool saturationSupported READ IsSaturationSupported NOTIFY filtersReady)
    Q_PROPERTY(bool noiseReductionSupported READ IsNoiseReductionSupported NOTIFY filtersReady)
    Q_PROPERTY(bool edgeEnhancementSupported READ IsEdgeEnhancementSupported NOTIFY filtersReady)

    Q_PROPERTY(int rotation READ GetRotation WRITE SetRotation NOTIFY rotationChanged)
public:
    explicit VideoDisplay(QQuickItem *parent = nullptr);
    ~VideoDisplay() override;

    int GetVideoWidth() const { return videoWidth; }
    int GetVideoHeight() const { return videoHeight; }

    QString GetSubtitleText() const { return subtitleText; }
    bool IsHDREnabled() const { return isHDREnabled; }

    qreal GetBrightness() const { return GetNormalizedFilterValue(0); }
    void SetBrightness(qreal val) { SetNormalizedFilterValue(0, val); }

    qreal GetContrast() const { return GetNormalizedFilterValue(1); }
    void SetContrast(qreal val) { SetNormalizedFilterValue(1, val); }

    qreal GetHue() const { return GetNormalizedFilterValue(2); }
    void SetHue(qreal val) { SetNormalizedFilterValue(2, val); }

    qreal GetSaturation() const { return GetNormalizedFilterValue(3); }
    void SetSaturation(qreal val) { SetNormalizedFilterValue(3, val); }

    qreal GetNoiseReduction() const { return GetNormalizedFilterValue(4); }
    void SetNoiseReduction(qreal val) { SetNormalizedFilterValue(4, val); }

    qreal GetEdgeEnhancement() const { return GetNormalizedFilterValue(5); }
    void SetEdgeEnhancement(qreal val) { SetNormalizedFilterValue(5, val); }

    bool IsBrightnessSupported() const { return filters[0].supported; }
    bool IsContrastSupported() const { return filters[1].supported; }
    bool IsHueSupported() const { return filters[2].supported; }
    bool IsSaturationSupported() const { return filters[3].supported; }
    bool IsNoiseReductionSupported() const { return filters[4].supported; }
    bool IsEdgeEnhancementSupported() const { return filters[5].supported; }

    int GetRotation() const { return rotation; }
    void SetRotation(int rot) {
        rot = rot % 360;
        if(rot < 0) rot += 360;

        if(rotation == rot) return;

        rotation = rot;
        emit rotationChanged();

        // 重绘当前帧
        if(cachedFrame && cachedFrame->data[0]) {
            RenderFrame(cachedFrame);
        }
    }


    // 恢复默认滤镜参数
    Q_INVOKABLE void ResetFilters();

    Q_INVOKABLE void SetHDREnabled(bool enable);
    Q_INVOKABLE void SetVisibility(bool visible);

    HWND GetDisplayHwnd() { return displayHwnd; };

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void PrepareFrame(AVFrame* frame, const std::string& subText) override;
    void Clear() override;

private:
    void InitDisplayWindow();
    bool InitD3D();
    bool InitVideoProcessor();
    void RenderFrame(AVFrame* frame);
    void RenderD3D11Frame(AVFrame* frame);
    void RenderSoftwareFrame(AVFrame* frame);
    void UpdateHDRMetaData(AVFrame* frame);
    DXGI_COLOR_SPACE_TYPE GetInputColorSpace(AVFrame* frame);
    // 应用滤镜
    void ApplyFilters();
    qreal GetNormalizedFilterValue(int idx) const;
    void SetNormalizedFilterValue(int idx, qreal val);

    // 滤镜参数
    struct FilterSettings {
        bool supported = false;
        D3D11_VIDEO_PROCESSOR_FILTER_RANGE range{};
        int currentVal = 0;
        qreal normalizedVal = 0.5;
    };
    FilterSettings filters[6];

    // 字幕文本
    QString subtitleText;

    // D3D11 视频处理
    ComPtr<ID3D11Device> pDevice;
    ComPtr<ID3D11DeviceContext> pContext;
    ComPtr<ID3D11VideoDevice> pVideoDevice;
    ComPtr<ID3D11VideoContext> pVideoContext;
    ComPtr<ID3D11VideoContext1> pVideoContext1;
    ComPtr<ID3D11VideoProcessor> pVideoProcessor;
    ComPtr<ID3D11VideoProcessorEnumerator> pVideoEnumerator;
    ComPtr<IDXGISwapChain1> pSwapChain1;
    ComPtr<IDXGISwapChain3> pSwapChain3;
    ComPtr<IDXGISwapChain4> pSwapChain4;

    // 软解
    ComPtr<ID3D11Texture2D> stagingTexture; // CPU 中转纹理
    ComPtr<ID3D11Texture2D> defaultTexture; // GPU 专用纹理
    SwsContext* swsCtx = nullptr;
    AVFrame* fmtConvFrame = nullptr;

    // Win32 子窗口句柄，真正显示视频的窗口
    HWND displayHwnd = nullptr;

    // 缓存帧用于重绘
    AVFrame* cachedFrame = nullptr;

    std::atomic<int> pendingFrames{0};

    int videoWidth{1920};
    int videoHeight{1080};

    bool isHDRSupported = false;
    bool isHDREnabled = true;

    bool videoChanged = false;

    int rotation = 0;

signals:
    void subtitleTextChanged();
    void hdrEnabledChanged();
    void videoSizeChanged();

    void filtersReady();
    void brightnessChanged();
    void contrastChanged();
    void hueChanged();
    void saturationChanged();
    void noiseReductionChanged();
    void edgeEnhancementChanged();

    void rotationChanged();

};

#endif // VIDEODISPLAY_H
