#include "VideoDisplay.h"
#include "XCPlayer.h"
#include <QQuickWindow>
extern "C"
{
#include "libavutil/mastering_display_metadata.h"
#include "libavutil/display.h"
#include "libswscale/swscale.h"
}

VideoDisplay::VideoDisplay(QQuickItem *parent)
    : QQuickItem{parent}
{
    setFlag(ItemHasContents, false);
    cachedFrame = av_frame_alloc();

    // (亮度/对比/色相/饱和)为 0.5，(降噪/锐化)为 0.0
    for(int i = 0; i < 6; ++i) {
        filters[i].normalizedVal = (i < 4) ? 0.5 : 0.0;
    }

    XCPlayer::GetInstance().SetVideoRenderer(this);

    connect(&XCPlayer::GetInstance(), &XCPlayer::playInfoChanged, this, [this]() {
        if(XCPlayer::GetInstance().GetPlayInfo().isVideo) {
            this->videoChanged = true;
            // 重置旋转
            this->rotation = 0;
            emit rotationChanged();
        }
    });
}

VideoDisplay::~VideoDisplay()
{
    XCPlayer::GetInstance().SetVideoRenderer(nullptr);

    if(cachedFrame) {
        av_frame_free(&cachedFrame);
    }

    if(fmtConvFrame) { av_frame_free(&fmtConvFrame); fmtConvFrame = nullptr; }
    if(swsCtx) { sws_freeContext(swsCtx); swsCtx = nullptr; }

    if(displayHwnd) {
        DestroyWindow(displayHwnd);
        displayHwnd = nullptr;
    }
}

void VideoDisplay::ResetFilters()
{
    for(int i = 0; i < 6; ++i) {
        if(filters[i].supported) {
            filters[i].currentVal = filters[i].range.Default;
            if(filters[i].range.Maximum != filters[i].range.Minimum) {
                filters[i].normalizedVal = (qreal)(filters[i].currentVal - filters[i].range.Minimum) /
                                           (qreal)(filters[i].range.Maximum - filters[i].range.Minimum);
            }
            if(pVideoContext && pVideoProcessor) {
                pVideoContext->VideoProcessorSetStreamFilter(pVideoProcessor.Get(), 0, (D3D11_VIDEO_PROCESSOR_FILTER)i, TRUE, filters[i].currentVal);
            }
        } else {
            filters[i].normalizedVal = (i < 4) ? 0.5 : 0.0;
        }
    }

    emit brightnessChanged();
    emit contrastChanged();
    emit hueChanged();
    emit saturationChanged();
    emit noiseReductionChanged();
    emit edgeEnhancementChanged();

    // 重绘当前帧
    if(cachedFrame && cachedFrame->data[0]) {
        RenderFrame(cachedFrame);
    }
}

void VideoDisplay::SetHDREnabled(bool enable)
{
    if(!isHDRSupported) isHDREnabled = false;

    if(isHDREnabled != enable) {
        isHDREnabled = enable;
        emit hdrEnabledChanged();

        // 重绘当前帧
        if(cachedFrame && cachedFrame->data[0]) {
            RenderFrame(cachedFrame);
        }
    }
}

void VideoDisplay::SetVisibility(bool visible)
{
    setVisible(visible);
    if(displayHwnd) {
        ShowWindow(displayHwnd, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
    }
}

void VideoDisplay::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);

    if(pSwapChain1 && newGeometry.width() > 0 && newGeometry.height() > 0) {
        // 计算物理像素
        qreal dpr = window()->devicePixelRatio();
        int w = std::round(newGeometry.width() * dpr);
        int h = std::round(newGeometry.height() * dpr);

        // 清除旧缓冲，重建新尺寸缓冲
        pContext->ClearState();
        pContext->Flush();
        pSwapChain1->ResizeBuffers(2, w, h, DXGI_FORMAT_UNKNOWN, 0);

        // displayHwnd 同步 QQuickItem 位置和大小
        if(displayHwnd && window()) {
            POINT pt = {0, 0};
            ClientToScreen((HWND)window()->winId(), &pt);
            QPointF scenePos = mapToScene(QPointF(0, 0));
            int targetX = pt.x + std::round(scenePos.x() * dpr);
            int targetY = pt.y + std::round(scenePos.y() * dpr);

            SetWindowPos(displayHwnd, (HWND)window()->winId(), targetX, targetY, w, h, SWP_NOACTIVATE | SWP_NOCOPYBITS);
        }

        // 重绘当前帧
        if(cachedFrame && cachedFrame->data[0]) {
            RenderFrame(cachedFrame);
        }
    }
}

void VideoDisplay::PrepareFrame(AVFrame *frame, const std::string &subText)
{
    if(!frame) return;

    // 防止主线程阻塞堆积过多待渲染帧
    if(pendingFrames.load(std::memory_order_acquire) >= 2) {
        return;
    }

    // 深拷贝
    AVFrame* clone = av_frame_clone(frame);
    if(!clone) return;

    QString text = QString::fromStdString(subText);

    pendingFrames.fetch_add(1, std::memory_order_release);

    // 移步主线程绘制
    QMetaObject::invokeMethod(this, [this, clone, text]() {
        pendingFrames.fetch_sub(1, std::memory_order_release);

        // 更新字幕
        if(subtitleText != text) {
            subtitleText = text;
            emit subtitleTextChanged();
        }

        // 绘制当前帧
        RenderFrame(clone);
        av_frame_free((AVFrame**)&clone);
    }, Qt::QueuedConnection);
}

void VideoDisplay::Clear()
{
    QMetaObject::invokeMethod(this, [this]() {
        if(cachedFrame) {
            av_frame_unref(cachedFrame);
        }
        if(fmtConvFrame) { av_frame_free(&fmtConvFrame); fmtConvFrame = nullptr; }
        if(swsCtx) { sws_freeContext(swsCtx); swsCtx = nullptr; }

        if(pSwapChain1 && pContext) {
            // 获取后缓冲纹理
            ComPtr<ID3D11Texture2D> backBuffer;
            if(SUCCEEDED(pSwapChain1->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer))) {
                // 创建渲染目标视图
                ComPtr<ID3D11RenderTargetView> rtv;
                pDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv);

                // 黑色填充后缓冲纹理并立即显示
                float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                pContext->ClearRenderTargetView(rtv.Get(), clearColor);
                pSwapChain1->Present(1, 0);
            }
        }
    }, Qt::QueuedConnection);
}

void VideoDisplay::InitDisplayWindow()
{
    if(displayHwnd || !window()) return;

    WNDCLASS wc{};
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"DisplayWindow";
    RegisterClass(&wc);

    qreal dpr = window()->devicePixelRatio();
    displayHwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP,    // 工具窗口，任务栏不显示 | 无法接收焦点 | 禁止创建重定向表面
        wc.lpszClassName, L"",
        WS_POPUP | WS_VISIBLE | WS_DISABLED,                                // 无边框的独立窗口 | 可视 | 窗口禁用无法接受用户输入
        mapToGlobal(0, 0).x(), mapToGlobal(0, 0).y(), width() * dpr, height() * dpr,
        nullptr, nullptr, wc.hInstance, nullptr
        );

    // 固定到 Qt 主窗口之下
    SetWindowPos(displayHwnd, (HWND)window()->winId(), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

bool VideoDisplay::InitD3D()
{
    if(!window() || window()->graphicsApi() != QSGRendererInterface::Direct3D11) return false;

    InitDisplayWindow();
    if(!displayHwnd) return false;

    // 复用 Qt Quick 底层的 D3D11 Device
    void* device = window()->rendererInterface()->getResource(window(), QSGRendererInterface::DeviceResource);
    if(!device) return false;
    pDevice = static_cast<ID3D11Device*>(device);
    pDevice->GetImmediateContext(&pContext);

    HRESULT hr;
    hr = pDevice.As(&pVideoDevice);
    if(FAILED(hr)) return false;

    hr = pContext.As(&pVideoContext);
    if(FAILED(hr)) return false;

    // 尝试获取 VideoContext1
    pContext.As(&pVideoContext1);


    // 创建 SwapChain
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;

    pDevice.As(&dxgiDevice);
    dxgiDevice->GetAdapter(&adapter);
    adapter->GetParent(__uuidof(IDXGIFactory2), (void**)&factory);

    // 配置交换链
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = qMax(1, (int)(width() * window()->devicePixelRatio()));
    sd.Height = qMax(1, (int)(height() * window()->devicePixelRatio()));
    sd.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
    sd.Stereo = FALSE;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2; // 3缓冲
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    hr = factory->CreateSwapChainForHwnd(pDevice.Get(), displayHwnd, &sd, nullptr, nullptr, &pSwapChain1);
    if(FAILED(hr)) return false;

    // 尝试获取 SwapChain3、4
    pSwapChain1.As(&pSwapChain3);
    pSwapChain1.As(&pSwapChain4);

    return true;
}

bool VideoDisplay::InitVideoProcessor()
{
    if(pVideoProcessor) {
        pVideoProcessor.Reset();
        pVideoEnumerator.Reset();
    }

    XC::BaseInfo info = XCPlayer::GetInstance().GetPlayInfo();
    uint num = 30, den = 1;
    if(info.num > 0 && info.den > 0) {
        num = XCPlayer::GetInstance().GetPlayInfo().num;
        den = XCPlayer::GetInstance().GetPlayInfo().den;
    }
    UINT w = info.width  > 0 ? info.width  : 1920;
    UINT h = info.height > 0 ? info.height : 1080;

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC contentDesc = {};
    contentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    contentDesc.InputFrameRate = {num, den};
    contentDesc.InputWidth = w;
    contentDesc.InputHeight = h;
    contentDesc.OutputWidth = width() * (window() ? window()->devicePixelRatio() : 1.0);
    contentDesc.OutputHeight = height() * (window() ? window()->devicePixelRatio() : 1.0);
    contentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    HRESULT hr = pVideoDevice->CreateVideoProcessorEnumerator(&contentDesc, &pVideoEnumerator);
    if(FAILED(hr)) return false;

    hr = pVideoDevice->CreateVideoProcessor(pVideoEnumerator.Get(), 0, &pVideoProcessor);
    if(FAILED(hr)) return false;

    // 应用滤镜
    ApplyFilters();

    return true;
}

void VideoDisplay::RenderFrame(AVFrame *frame)
{
    if(!frame) return;

    if(!pDevice) {
        if(!InitD3D()) return;
    }

    if(!pSwapChain1) return;

    // 视频切换时重建VideoProcessor
    if(!pVideoProcessor || videoChanged) {
        if(!InitVideoProcessor()) return;
        videoChanged = false;
    }

    // 更新分辨率给 QML
    if(videoWidth != frame->width || videoHeight != frame->height) {
        videoWidth = frame->width;
        videoHeight = frame->height;
        emit videoSizeChanged();
    }

    // 缓存当前帧
    if(frame != cachedFrame) {
        av_frame_unref(cachedFrame);
        av_frame_ref(cachedFrame, frame);
    }

    // 更新 HDR 数据
    UpdateHDRMetaData(frame);

    // 区分硬解和软解
    if(frame->format == AV_PIX_FMT_D3D11) {
        RenderD3D11Frame(frame);
    } else {
        RenderSoftwareFrame(frame);
    }

    // 等待垂直同步
    pSwapChain1->Present(1, 0);
}

void VideoDisplay::RenderD3D11Frame(AVFrame *frame)
{
    // 获取解码纹理和纹理数组索引
    ID3D11Texture2D* srcTexture = (ID3D11Texture2D*)frame->data[0];
    intptr_t arrayIndex = (intptr_t)frame->data[1];
    if(!srcTexture) return;

    // 获取后缓冲纹理
    ComPtr<ID3D11Texture2D> backBuffer;
    pSwapChain1->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);

    // 创建视频处理器的输入/输出视图
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc{};
    inDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    inDesc.Texture2D.ArraySlice = (UINT)arrayIndex;
    ComPtr<ID3D11VideoProcessorInputView> inputView;
    pVideoDevice->CreateVideoProcessorInputView(srcTexture, pVideoEnumerator.Get(), &inDesc, &inputView);

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc{};
    outDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    ComPtr<ID3D11VideoProcessorOutputView> outputView;
    pVideoDevice->CreateVideoProcessorOutputView(backBuffer.Get(), pVideoEnumerator.Get(), &outDesc, &outputView);

    if(!inputView || !outputView) return;


    // 判断是否为隔行扫描
    bool isInterlaced = (frame->flags & AV_FRAME_FLAG_INTERLACED) != 0;
    bool isTFF = (frame->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST) != 0;
    D3D11_VIDEO_FRAME_FORMAT d3dFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    if(isInterlaced) {
        d3dFrameFormat = isTFF ? D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST
                               : D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
    }
    pVideoContext->VideoProcessorSetStreamFrameFormat(pVideoProcessor.Get(), 0, d3dFrameFormat);

    // 色彩空间设置
    if(pVideoContext1) {
        DXGI_COLOR_SPACE_TYPE inputColorSpace = GetInputColorSpace(frame);
        pVideoContext1->VideoProcessorSetStreamColorSpace1(pVideoProcessor.Get(), 0, inputColorSpace);

        DXGI_COLOR_SPACE_TYPE outputColorSpace{};
        bool isHDRVideo = (frame->color_trc == AVCOL_TRC_SMPTE2084 || frame->color_trc == AVCOL_TRC_ARIB_STD_B67);
        if(isHDRSupported && isHDREnabled && isHDRVideo) {
            outputColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
        } else {
            bool isBT2020 = (frame->colorspace == AVCOL_SPC_BT2020_NCL || frame->colorspace == AVCOL_SPC_BT2020_CL ||
                             frame->color_primaries == AVCOL_PRI_BT2020);

            if(isBT2020) {
                outputColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020;
            } else {
                outputColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
            }
        }
        pVideoContext1->VideoProcessorSetOutputColorSpace1(pVideoProcessor.Get(), outputColorSpace);
    } else {
        D3D11_VIDEO_PROCESSOR_COLOR_SPACE colorSpace{};
        colorSpace.Usage = 0;
        colorSpace.RGB_Range = (frame->color_range == AVCOL_RANGE_JPEG) ? 0 : 1;
        bool isBT709 = (frame->colorspace == AVCOL_SPC_BT709 || frame->color_primaries == AVCOL_PRI_BT709);
        colorSpace.YCbCr_Matrix = isBT709 ? 1 : 0;
        pVideoContext->VideoProcessorSetStreamColorSpace(pVideoProcessor.Get(), 0, &colorSpace);
        pVideoContext->VideoProcessorSetOutputColorSpace(pVideoProcessor.Get(), &colorSpace);
    }

    // 旋转设置
    int finalRot = rotation;
    AVFrameSideData* sdMatrix = av_frame_get_side_data(frame, AV_FRAME_DATA_DISPLAYMATRIX);
    if(sdMatrix) {
        double metaRotation = av_display_rotation_get((int32_t*)sdMatrix->data);
        if(!std::isnan(metaRotation)) {
            // 吸附到最近的 90 度倍数，避免奇怪的旋转角度
            int angle = (int)std::round(metaRotation / 90.0) * 90;

            // 减去 angle 才是恢复正常角度
            finalRot -= angle;
        }
    }

    finalRot %= 360;
    if(finalRot < 0) finalRot += 360;

    D3D11_VIDEO_PROCESSOR_ROTATION d3dRot{D3D11_VIDEO_PROCESSOR_ROTATION_IDENTITY};
    if(finalRot == 90) d3dRot = D3D11_VIDEO_PROCESSOR_ROTATION_90;
    else if(finalRot == 180) d3dRot = D3D11_VIDEO_PROCESSOR_ROTATION_180;
    else if(finalRot == 270) d3dRot = D3D11_VIDEO_PROCESSOR_ROTATION_270;
    pVideoContext->VideoProcessorSetStreamRotation(pVideoProcessor.Get(), 0, TRUE, d3dRot);

    // 自适应等比缩放与居中显示
    qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
    int winW = width() * dpr;
    int winH = height() * dpr;
    int vidW = frame->width;
    int vidH = frame->height;

    // 如果旋转了 90 或 270 度，目标区域的宽高对调
    if(finalRot == 90 || finalRot == 270) {
        std::swap(vidW, vidH);
    }

    if(vidW == 0 || vidH == 0 || winW == 0 || winH == 0) return;

    float winRatio = (float)winW / winH;
    float vidRatio = (float)vidW / vidH;

    int newW, newH, x, y;
    // 窗口更宽
    if(winRatio > vidRatio) {
        newH = winH;
        newW = (int)(newH * vidRatio);
        x = (winW - newW) / 2;
        y = 0;
    }
    // 窗口更窄
    else {
        newW = winW;
        newH = (int)(newW / vidRatio);
        x = 0;
        y = (winH - newH) / 2;
    }

    RECT srcRect{ 0, 0, frame->width, frame->height };
    RECT destRect{ x, y, x + newW, y + newH };
    RECT outputRect{ 0, 0, winW, winH };

    // 设置输入区域（视频原始大小不裁剪）
    pVideoContext->VideoProcessorSetStreamSourceRect(pVideoProcessor.Get(), 0, TRUE, &srcRect);
    // 设置目标绘制区域（等比缩放和居中）
    pVideoContext->VideoProcessorSetStreamDestRect(pVideoProcessor.Get(), 0, TRUE, &destRect);
    // 设置输出区域（窗口大小）
    pVideoContext->VideoProcessorSetOutputTargetRect(pVideoProcessor.Get(), TRUE, &outputRect);

    // 黑色背景
    D3D11_VIDEO_COLOR bgColor{};
    bgColor.RGBA = {0.0f, 0.0f, 0.0f, 1.0f};
    pVideoContext->VideoProcessorSetOutputBackgroundColor(pVideoProcessor.Get(), 0, &bgColor);

    // 开始处理视频
    D3D11_VIDEO_PROCESSOR_STREAM stream{};
    stream.Enable = TRUE;
    stream.pInputSurface = inputView.Get();
    pVideoContext->VideoProcessorBlt(pVideoProcessor.Get(), outputView.Get(), 0, 1, &stream);
}

void VideoDisplay::RenderSoftwareFrame(AVFrame *frame)
{
    // 宽高必须是偶数
    bool isEvenDim = (frame->width % 2 == 0) && (frame->height % 2 == 0);
    bool isAlready = isEvenDim && (frame->format == AV_PIX_FMT_NV12 || frame->format == AV_PIX_FMT_P010LE);
    AVFrame* uploadFrame = frame;

    // 转换为 NV12 或 PO10 格式
    if(!isAlready) {
        bool is10Bit = (frame->format == AV_PIX_FMT_YUV420P10LE || frame->format == AV_PIX_FMT_YUV444P10LE);
        AVPixelFormat targetFmt = is10Bit ? AV_PIX_FMT_P010LE : AV_PIX_FMT_NV12;

        // 计算对齐后的偶数宽高
        int texWidth = (frame->width + 1) & ~1;
        int texHeight = (frame->height + 1) & ~1;

        // 目标格式或尺寸变化时重建 SWS 上下文 和 纹理数据
        if(!swsCtx ||
            fmtConvFrame->width != texWidth  ||
            fmtConvFrame->height != texHeight  ||
            fmtConvFrame->format != targetFmt)
        {
            if(fmtConvFrame) { av_frame_free(&fmtConvFrame); fmtConvFrame = nullptr; }
            if(swsCtx) { sws_freeContext(swsCtx); swsCtx = nullptr; }

            fmtConvFrame = av_frame_alloc();
            fmtConvFrame->format = targetFmt;
            fmtConvFrame->width = texWidth;
            fmtConvFrame->height = texHeight;

            // 使用 av_frame_get_buffer 根据format、width 和 height自动分配 data
            if(av_frame_get_buffer(fmtConvFrame, 0) < 0) {
                av_frame_free(&fmtConvFrame);
                return;
            }

            // 创建 SWS 上下文进行尺寸和格式转换
            swsCtx = sws_getContext(frame->width, frame->height, (AVPixelFormat)frame->format,
                                    frame->width, frame->height, targetFmt,
                                    SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

            stagingTexture.Reset();
            defaultTexture.Reset();
        }

        // 转换为 GPU 友好的 NV12/P010 中转格式
        sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height, fmtConvFrame->data, fmtConvFrame->linesize);

        // 拷贝原始帧的元数据
        av_frame_copy_props(fmtConvFrame, frame);

        uploadFrame = fmtConvFrame;
    }

    if(!defaultTexture) {
        // 创建纹理
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = uploadFrame->width;
        desc.Height = uploadFrame->height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = uploadFrame->format == AV_PIX_FMT_P010LE ? DXGI_FORMAT_P010 : DXGI_FORMAT_NV12;
        desc.SampleDesc.Count = 1;

        // GPU 专用纹理，供 VideoProcessor 读取和写入
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        HRESULT hr1 = pDevice->CreateTexture2D(&desc, nullptr, &defaultTexture);

        // CPU 中转纹理，用于 CPU 数据上传至显存
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0; // Staging 资源无法绑定到渲染管线
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        HRESULT hr2 = pDevice->CreateTexture2D(&desc, nullptr, &stagingTexture);

        if(FAILED(hr1) || FAILED(hr2) || !stagingTexture || !defaultTexture) {
            return;
        }
    }

    D3D11_MAPPED_SUBRESOURCE map;
    // 将 staging 纹理地址映射到 CPU 可访问的内存地址
    if(SUCCEEDED(pContext->Map(stagingTexture.Get(), 0, D3D11_MAP_WRITE, 0, &map))) {
        // 计算真实像素的字节宽度 (P010每个像素2字节，NV12每个像素1字节)
        int bytesPerPixel = (uploadFrame->format == AV_PIX_FMT_P010LE) ? 2 : 1;
        int copyWidth = uploadFrame->width * bytesPerPixel;

        // 拷贝 Y 平面
        uint8_t* srcY = uploadFrame->data[0];
        uint8_t* dstY = (uint8_t*)map.pData;
        for(int i = 0; i < uploadFrame->height; i++) {
            // 严防越界：取 FFmpeg linesize、D3D11 RowPitch 和 真实像素宽度 中的最小值
            int copyBytes = std::min({(int)uploadFrame->linesize[0], (int)map.RowPitch, copyWidth});
            memcpy(dstY + i * map.RowPitch, srcY + i * uploadFrame->linesize[0], copyBytes);
        }

        // 拷贝 UV 平面
        uint8_t* srcUV = uploadFrame->data[1];
        uint8_t* dstUV = dstY + map.RowPitch * uploadFrame->height; // UV 数据紧跟在 Y 数据后
        int uvHeight = uploadFrame->height / 2; // UV 高度是 Y 的一半
        for(int i = 0; i < uvHeight; i++) {
            // NV12/P010 的 UV 平面字节宽度与 Y 平面相同 (因为是 U 和 V 交错的)
            int copyBytes = std::min({(int)uploadFrame->linesize[1], (int)map.RowPitch, copyWidth});
            memcpy(dstUV + i * map.RowPitch, srcUV + i * uploadFrame->linesize[1], copyBytes);
        }

        // 解除映射并拷贝 stagingTexture 给 defaultTexture
        pContext->Unmap(stagingTexture.Get(), 0);
        pContext->CopyResource(defaultTexture.Get(), stagingTexture.Get());

        // D3D11 帧封装，栈上分配
        AVFrame d3dFrame;
        memset(&d3dFrame, 0, sizeof(d3dFrame));
        d3dFrame.format = AV_PIX_FMT_D3D11;
        d3dFrame.width  = uploadFrame->width;
        d3dFrame.height = uploadFrame->height;
        d3dFrame.data[0] = (uint8_t*)defaultTexture.Get();
        d3dFrame.data[1] = (uint8_t*)0;

        // 拷贝元数据（包括色彩信息和 side data）
        av_frame_copy_props(&d3dFrame, uploadFrame);

        RenderD3D11Frame(&d3dFrame);

        // 清理 side data 内存
        av_frame_unref(&d3dFrame);
    }
}

void VideoDisplay::UpdateHDRMetaData(AVFrame *frame)
{
    if(!pSwapChain1 || !frame) return;

    bool isPQ = (frame->color_trc == AVCOL_TRC_SMPTE2084);
    bool isHLG = (frame->color_trc == AVCOL_TRC_ARIB_STD_B67);

    if(pSwapChain3) {
        // 检查显示器是否处于 Windows HDR 开启状态
        UINT support = 0;
        if (SUCCEEDED(pSwapChain3->CheckColorSpaceSupport(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, &support)) &&
            (support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)) {
            isHDRSupported = true;
        } else {
            isHDRSupported = false;
        }
    }

    // 设置色彩空间
    if(pSwapChain3 && isHDRSupported && isHDREnabled && (isPQ || isHLG)) {
        pSwapChain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
    } else if(pSwapChain3) {
        pSwapChain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
    }

    // 设置 HDR10 元数据
    if(pSwapChain4) {
        if(isPQ && isHDRSupported && isHDREnabled) {
            DXGI_HDR_METADATA_HDR10 hdr10{};

            // 获取 SMPTE ST 2086 元数据
            AVFrameSideData* sd = av_frame_get_side_data(frame, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
            if(sd) {
                AVMasteringDisplayMetadata* mastering = (AVMasteringDisplayMetadata*)sd->data;
                if(mastering->has_primaries) {
                    auto q2d_safe =[](AVRational r) { return r.den == 0 ? 0.0 : av_q2d(r); };
                    hdr10.RedPrimary[0]   = (UINT16)round(q2d_safe(mastering->display_primaries[0][0]) * 50000);
                    hdr10.RedPrimary[1]   = (UINT16)round(q2d_safe(mastering->display_primaries[0][1]) * 50000);
                    hdr10.GreenPrimary[0] = (UINT16)round(q2d_safe(mastering->display_primaries[1][0]) * 50000);
                    hdr10.GreenPrimary[1] = (UINT16)round(q2d_safe(mastering->display_primaries[1][1]) * 50000);
                    hdr10.BluePrimary[0]  = (UINT16)round(q2d_safe(mastering->display_primaries[2][0]) * 50000);
                    hdr10.BluePrimary[1]  = (UINT16)round(q2d_safe(mastering->display_primaries[2][1]) * 50000);
                    hdr10.WhitePoint[0]   = (UINT16)round(q2d_safe(mastering->white_point[0]) * 50000);
                    hdr10.WhitePoint[1]   = (UINT16)round(q2d_safe(mastering->white_point[1]) * 50000);
                }
                if(mastering->has_luminance) {
                    auto q2d_safe =[](AVRational r) { return r.den == 0 ? 0.0 : av_q2d(r); };
                    hdr10.MaxMasteringLuminance  = (UINT)round(q2d_safe(mastering->max_luminance) * 10000);
                    hdr10.MinMasteringLuminance  = (UINT)round(q2d_safe(mastering->min_luminance) * 10000);
                }
            }

            // 获取内容亮度级别
            AVFrameSideData* cll_sd = av_frame_get_side_data(frame, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
            if(cll_sd) {
                AVContentLightMetadata* cll = (AVContentLightMetadata*)cll_sd->data;
                hdr10.MaxContentLightLevel      = (UINT16)cll->MaxCLL;
                hdr10.MaxFrameAverageLightLevel = (UINT16)cll->MaxFALL;
            }

            // 设置 HDR10 静态元数据
            pSwapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &hdr10);

        } else {
            // 清除元数据
            pSwapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_NONE, 0, nullptr);
        }
    }
}

DXGI_COLOR_SPACE_TYPE VideoDisplay::GetInputColorSpace(AVFrame *frame)
{
    // 判断全范围
    bool isFullRange = (frame->color_range == AVCOL_RANGE_JPEG);

    // 判断色度采样位置
    bool isTopLeft = (frame->chroma_location == AVCHROMA_LOC_TOPLEFT);

    // 判断色彩空间矩阵或原色
    bool isBT2020 = (frame->colorspace == AVCOL_SPC_BT2020_NCL || frame->colorspace == AVCOL_SPC_BT2020_CL ||
                     frame->color_primaries == AVCOL_PRI_BT2020);
    bool isBT601  = (frame->colorspace == AVCOL_SPC_BT470BG || frame->colorspace == AVCOL_SPC_SMPTE170M ||
                    frame->color_primaries == AVCOL_PRI_BT470BG || frame->color_primaries == AVCOL_PRI_SMPTE170M);

    // 判断传输特性
    bool isPQ  = (frame->color_trc == AVCOL_TRC_SMPTE2084);
    bool isHLG = (frame->color_trc == AVCOL_TRC_ARIB_STD_B67);

    // 处理 BT.2020
    if(isBT2020) {
        // BT.2020 HDR
        if(isPQ) {
            return isTopLeft ? DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020
                             : DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020;
        }

        if(isHLG) {
            if(isFullRange) {
                return DXGI_COLOR_SPACE_YCBCR_FULL_GHLG_TOPLEFT_P2020;
            } else {
                return DXGI_COLOR_SPACE_YCBCR_STUDIO_GHLG_TOPLEFT_P2020;
            }
        }

        // BT.2020 SDR
        if(isFullRange) {
            return DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020;
        } else {
            return isTopLeft ? DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_TOPLEFT_P2020
                             : DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020;
        }
    }
    // 处理 BT.601
    else if(isBT601) {
        return isFullRange ? DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601
                           : DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601;
    }
    // 默认回退 BT.709
    else {
        return isFullRange ? DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709
                           : DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709;
    }
}

void VideoDisplay::ApplyFilters()
{
    if(!pVideoContext || !pVideoProcessor || !pVideoEnumerator) return;

    // 开启自动处理模式
    pVideoContext->VideoProcessorSetStreamAutoProcessingMode(pVideoProcessor.Get(), 0, TRUE);

    auto updateFilter = [&](int idx, D3D11_VIDEO_PROCESSOR_FILTER filter, auto emitSignal) {
        if (SUCCEEDED(pVideoEnumerator->GetVideoProcessorFilterRange(filter, &filters[idx].range))) {
            filters[idx].supported = true;

            filters[idx].currentVal = filters[idx].range.Minimum +
                                      qRound(filters[idx].normalizedVal * (filters[idx].range.Maximum - filters[idx].range.Minimum));

            pVideoContext->VideoProcessorSetStreamFilter(pVideoProcessor.Get(), 0, filter, TRUE, filters[idx].currentVal);

            // 通知 UI 数值更新了
            emitSignal();
        } else {
            filters[idx].supported = false;
        }
    };

    updateFilter(0, D3D11_VIDEO_PROCESSOR_FILTER_BRIGHTNESS, [this](){ emit brightnessChanged(); });
    updateFilter(1, D3D11_VIDEO_PROCESSOR_FILTER_CONTRAST, [this](){ emit contrastChanged(); });
    updateFilter(2, D3D11_VIDEO_PROCESSOR_FILTER_HUE, [this](){ emit hueChanged(); });
    updateFilter(3, D3D11_VIDEO_PROCESSOR_FILTER_SATURATION, [this](){ emit saturationChanged(); });
    updateFilter(4, D3D11_VIDEO_PROCESSOR_FILTER_NOISE_REDUCTION,[this](){ emit noiseReductionChanged(); });
    updateFilter(5, D3D11_VIDEO_PROCESSOR_FILTER_EDGE_ENHANCEMENT, [this](){ emit edgeEnhancementChanged(); });

    // 通知 UI 所有 filter 准备好了
    emit filtersReady();
}

qreal VideoDisplay::GetNormalizedFilterValue(int idx) const
{
    if(!filters[idx].supported || filters[idx].range.Maximum == filters[idx].range.Minimum) {
        return filters[idx].normalizedVal;
    }
    return (qreal)(filters[idx].currentVal - filters[idx].range.Minimum) /
           (qreal)(filters[idx].range.Maximum - filters[idx].range.Minimum);
}

void VideoDisplay::SetNormalizedFilterValue(int idx, qreal val)
{
    val = qBound(0.0, val, 1.0);

    // 误差小于百万分之一就判断为相等
    if(qAbs(filters[idx].normalizedVal - val) < 1e-6) {
        return;
    }

    filters[idx].normalizedVal = val;

    if(filters[idx].supported) {
        int realVal = filters[idx].range.Minimum + qRound(val * (filters[idx].range.Maximum - filters[idx].range.Minimum));
        int clampedVal = qBound(filters[idx].range.Minimum, realVal, filters[idx].range.Maximum);

        if(filters[idx].currentVal != clampedVal) {
            filters[idx].currentVal = clampedVal;
            pVideoContext->VideoProcessorSetStreamFilter(pVideoProcessor.Get(), 0, (D3D11_VIDEO_PROCESSOR_FILTER)idx, TRUE, clampedVal);

            // 重绘当前帧
            if(cachedFrame && cachedFrame->data[0]) {
                RenderFrame(cachedFrame);
            }
        }
    }

    // 通知 UI 数值更新了
    switch(idx) {
    case 0: emit brightnessChanged(); break;
    case 1: emit contrastChanged(); break;
    case 2: emit hueChanged(); break;
    case 3: emit saturationChanged(); break;
    case 4: emit noiseReductionChanged(); break;
    case 5: emit edgeEnhancementChanged(); break;
    }
}
