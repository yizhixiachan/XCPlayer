#ifndef NATIVEEVENTFILTER_H
#define NATIVEEVENTFILTER_H

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <windows.h>

class QWindow;
class VideoDisplay;

class NativeEventFilter : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit NativeEventFilter(QObject* parent = nullptr) : QObject(parent) {}

public slots:
    void SetWindow(QWindow* w);
    void SetVideoDisplay(VideoDisplay* vd);
    void SyncPosition();

    void ShowMinimize() {
        if(hwnd) ShowWindow(hwnd, SW_MINIMIZE);
    }
    void ShowMaximize() {
        if(hwnd) ShowWindow(hwnd, SW_MAXIMIZE);
    }
    void ShowRestore() {
        if(hwnd) ShowWindow(hwnd, SW_RESTORE);
    }

protected:
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    QWindow* window{nullptr};
    HWND hwnd{nullptr};
    VideoDisplay* videoDisplay{nullptr};
};

#endif // NATIVEEVENTFILTER_H
