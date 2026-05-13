#include "NativeEventFilter.h"
#include "quickitem/VideoDisplay.h"
#include <windowsx.h>
#include <QWindow>

void NativeEventFilter::SetWindow(QWindow *w)
{
    if(!w) return;
    window = w;

    hwnd = (HWND)window->winId();

    // 保留 Split Screen & Snap Assist 功能
    LONG style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style |= WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
}

void NativeEventFilter::SetVideoDisplay(VideoDisplay *vd)
{
    if(!vd) return;
    videoDisplay = vd;
}

void NativeEventFilter::SyncPosition()
{
    if(!window || !videoDisplay || !IsWindow(videoDisplay->GetDisplayHwnd())) return;

    qreal dpr = window->devicePixelRatio();

    QPointF scenePos = videoDisplay->mapToScene(QPointF(0, 0));

    POINT pt = {0, 0};
    ClientToScreen(hwnd, &pt);
    // 计算物理坐标
    int targetX = pt.x + std::round(scenePos.x() * dpr);
    int targetY = pt.y + std::round(scenePos.y() * dpr);

    int w = std::round(videoDisplay->width() * dpr);
    int h = std::round(videoDisplay->height() * dpr);

    // 同步位置
    SetWindowPos(videoDisplay->GetDisplayHwnd(), hwnd, targetX, targetY, w, h,
                 SWP_NOACTIVATE | SWP_NOCOPYBITS);
}

bool NativeEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    if(!window) return false;
    MSG* msg = static_cast<MSG*>(message);
    if(msg->hwnd != hwnd) return false;

    switch(msg->message) {
    case WM_NCCALCSIZE: {
        *result = 0;
        return true;
    }

    case WM_NCACTIVATE: {
        *result = TRUE;
        return true;
    }

    case WM_NCHITTEST: {
        bool isMaximized = window->windowState() & Qt::WindowMaximized;
        bool isFullScreen = window->windowState() & Qt::WindowFullScreen;
        // 获取鼠标当前的物理坐标
        int x = GET_X_LPARAM(msg->lParam);
        int y = GET_Y_LPARAM(msg->lParam);

        // 获取窗口的物理矩形
        RECT rcWindow;
        GetWindowRect(hwnd, &rcWindow);

        // 窗口边缘缩放
        if(!(isMaximized || isFullScreen)) {
            int border = 4;

            bool isLeft   = x < rcWindow.left + border;
            bool isRight  = x > rcWindow.right - border;
            bool isTop    = y < rcWindow.top + border;
            bool isBottom = y > rcWindow.bottom - border;

            if(isLeft && isTop)     { *result = HTTOPLEFT; return true; }
            if(isRight && isTop)    { *result = HTTOPRIGHT; return true; }
            if(isLeft && isBottom)  { *result = HTBOTTOMLEFT; return true; }
            if(isRight && isBottom) { *result = HTBOTTOMRIGHT; return true; }
            if(isLeft)              { *result = HTLEFT; return true; }
            if(isRight)             { *result = HTRIGHT; return true; }
            if(isTop)               { *result = HTTOP; return true; }
            if(isBottom)            { *result = HTBOTTOM; return true; }
        }

        // 标题栏拖动、双击
        qreal dpr = window->devicePixelRatio();
        int titleBarHeightPhys = std::round(38 * dpr);
        int leftBtnWidthPhys = std::round(90 * dpr);
        int rightBtnWidthPhys = std::round(138 * dpr);

        if(!isFullScreen && y < rcWindow.top + titleBarHeightPhys) {
            bool inLeftButtons = x <= rcWindow.left + leftBtnWidthPhys;
            bool inRightButtons = x >= rcWindow.right - rightBtnWidthPhys;
            if(!inLeftButtons && !inRightButtons) {
                *result = HTCAPTION;
                return true;
            }
        }

        *result = HTCLIENT;
        return true;
    }

    case WM_NCLBUTTONDOWN: {
        if(msg->wParam == HTCAPTION) {
            QPoint globalPos(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
            QPointF localPos = window->mapFromGlobal(globalPos);

            // 伪造一个 Qt 的鼠标点击事件发给 Q t窗口
            QMouseEvent pressEvent(QEvent::MouseButtonPress, localPos, globalPos,
                                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(window, &pressEvent);

            QMouseEvent releaseEvent(QEvent::MouseButtonRelease, localPos, globalPos,
                                     Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(window, &releaseEvent);
        }
        break;
    }

    // 同步视频窗口
    case WM_WINDOWPOSCHANGED: {
        SyncPosition();
        break;
    }
    }

    return false;
}
