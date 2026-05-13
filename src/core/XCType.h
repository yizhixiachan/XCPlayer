#ifndef XCTYPE_H
#define XCTYPE_H

#include <deque>
#include <condition_variable>
#include <atomic>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include "libavfilter/avfilter.h"
}
class AVFilterGraph;
class AVFilterContext;

template<typename T, void (Func)(T**)>
struct AVDeleter {
    void operator()(T* p) const { if(p) Func(&p); }
};

template<typename T, void (Func)(T*)>
struct AVDeleterOnePtr {
    void operator()(T* p) const { if(p) Func(p); }
};

using AVPacketPtr = std::unique_ptr<AVPacket, AVDeleter<AVPacket, av_packet_free>>;
using AVFramePtr = std::unique_ptr<AVFrame, AVDeleter<AVFrame, av_frame_free>>;
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVDeleter<AVCodecContext, avcodec_free_context>>;
using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVDeleter<AVFormatContext, avformat_close_input>>;

struct PacketUnit {
    AVPacketPtr pkt;
    bool bFlushReq{false};
    size_t data_size{0};
    int serial{0};
};

struct FrameUnit {
    AVFramePtr frame;
    std::string sub_text;
    double pts = 0;
    double duration = 0;
    bool auto_step = false; // 标记此帧是否为Seek后的首帧，需强制渲染
    size_t data_size{0};
    double speed = 1.0;
    int serial{0};

    FrameUnit() = default;
    FrameUnit(FrameUnit&&) = default;
    FrameUnit& operator=(FrameUnit&&) = default;
    FrameUnit(const FrameUnit&) = delete;
    FrameUnit& operator=(const FrameUnit&) = delete;
};

template<typename T>
class Queue {
public:
    void Push(T&& item) {
        std::lock_guard<std::mutex> lock(mutex);
        mem_size += item.data_size;
        queue.push_back(std::move(item));
        cv.notify_one();
    }

    bool Pop(T& item, std::atomic<bool>& abort) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return !queue.empty() || abort.load(); });
        if(abort.load() || queue.empty()) return false;
        item = std::move(queue.front());
        mem_size -= item.data_size;
        queue.pop_front();
        return true;
    }

    bool TryPop(T& item) {
        std::lock_guard<std::mutex> lock(mutex);
        if(queue.empty()) return false;
        item = std::move(queue.front());
        mem_size -= item.data_size;
        queue.pop_front();
        return true;
    }

    void Flush() {
        std::lock_guard<std::mutex> lock(mutex);
        queue.clear();
        mem_size = 0;
        Wake();
    }

    void Wake() { cv.notify_all(); }
    size_t Size() { std::lock_guard<std::mutex> lock(mutex); return queue.size(); }
    size_t MemSize() { std::lock_guard<std::mutex> lock(mutex); return mem_size; }
private:
    std::deque<T> queue;
    size_t mem_size{0};
    std::mutex mutex;
    std::condition_variable cv;
};

class Clock
{
public:
    Clock() { Reset(); }

    void Reset() {
        std::lock_guard<std::mutex> lock(mtx);
        pts = 0.0;
        last_updated_time = av_gettime_relative() / 1000000.0;
        bPaused = false;
        speed = 1.0;
        serial = 0;
    }

    void Init(std::atomic<int>* serial) {
        std::lock_guard<std::mutex> lock(mtx);
        globalSerial = serial;
    }

    void Set(double pts, int frameSerial) {
        std::lock_guard<std::mutex> lock(mtx);

        if(globalSerial && frameSerial != globalSerial->load(std::memory_order_acquire)) {
            return;
        }

        this->pts = pts;
        this->serial = frameSerial;
        last_updated_time = av_gettime_relative() / 1000000.0;
    }

    double Get() const {
        std::lock_guard<std::mutex> lock(mtx);

        if(globalSerial && serial != globalSerial->load(std::memory_order_acquire)) {
            return pts;
        }

        if(bPaused) return pts;

        double now = av_gettime_relative() / 1000000.0;
        return pts + (now - last_updated_time) * speed;
    }

    void Pause(bool pause) {
        std::lock_guard<std::mutex> lock(mtx);
        if(bPaused == pause) return;

        double now = av_gettime_relative() / 1000000.0;
        if(pause) {
            if(!globalSerial || serial == globalSerial->load(std::memory_order_acquire)) {
                pts = pts + (now - last_updated_time) * speed;
            }
        } else {
            last_updated_time = now;
        }
        bPaused = pause;
    }

    void SetSpeed(double new_speed) {
        std::lock_guard<std::mutex> lock(mtx);
        if(speed == new_speed) return;

        double now = av_gettime_relative() / 1000000.0;
        if(!bPaused) {
            if(!globalSerial || serial == globalSerial->load(std::memory_order_acquire)) {
                pts = pts + (now - last_updated_time) * speed;
            }
            last_updated_time = now;
        }
        speed = new_speed;
    }

private:
    double pts{0.0};
    double last_updated_time{0.0};
    double speed{1.0};
    bool bPaused{false};
    int serial{0};
    std::atomic<int>* globalSerial{nullptr};
    mutable std::mutex mtx;
};

struct Decoder {
    AVCodecContextPtr avctxPtr;
    Queue<PacketUnit> pkt_queue{};
    Queue<FrameUnit> frame_queue{};
    std::atomic<bool> bFinished{false};
    AVRational time_base{1, 1000};

    AVFilterGraph* filter_graph = nullptr;
    AVFilterContext* filter_src_ctx = nullptr;
    AVFilterContext* filter_sink_ctx = nullptr;

    AVBufferRef* hw_device_ctx = nullptr;
    AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;


    void Flush() {
        pkt_queue.Flush();
        frame_queue.Flush();
        bFinished = false;
    }

    void Reset() {
        Flush();
        FreeFilter();
        avctxPtr.reset();
    }

    void Wake() {
        pkt_queue.Wake();
        frame_queue.Wake();
    }

    void FreeFilter() {
        if(filter_graph) {
            filter_src_ctx = nullptr;
            filter_sink_ctx = nullptr;
            avfilter_graph_free(&filter_graph);
        }
    }

    ~Decoder() {
        if(hw_device_ctx) av_buffer_unref(&hw_device_ctx);
        FreeFilter();
    }
};

class IVideoRenderer {
public:
    virtual ~IVideoRenderer() = default;

    virtual void PrepareFrame(AVFrame* frame, const std::string& subText) = 0;
    virtual void Clear() = 0;
};

#endif
