#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <future>

using Task = std::function<void()>;

struct TaskDeque {
    std::deque<Task> tasks;
    std::mutex mutex;
};

class ThreadPool
{
public:
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    static ThreadPool& GetInstance() {
        static ThreadPool inst;
        return inst;
    }

    template<class F, class... Args>
    auto PushTask_WithFuture(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;

    template<class F, class... Args>
    void PushTask(F&& f, Args&&... args);

    int GetThreadCount() const { return workers.size(); }
    int GetPendingTaskCount() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        return tasks.size();
    }

private:
    ThreadPool() {
        unsigned int hw_threads = std::thread::hardware_concurrency();
        unsigned int threads = std::max(2u, std::min(hw_threads > 0 ? hw_threads - 1 : 2u, 16u));

        for(size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                while(true) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock, [this] {
                            return bStopReq.load(std::memory_order_acquire) || !tasks.empty();
                        });

                        if(bStopReq.load(std::memory_order_acquire) && tasks.empty()) {
                            return;
                        }

                        task = std::move(tasks.front());
                        tasks.pop();
                    }

                    if(task) {
                        task();
                    }
                }
            });
        }
    }

    ~ThreadPool() {
        bStopReq.store(true, std::memory_order_release);
        condition.notify_all();
        for(std::thread& worker : workers) {
            if(worker.joinable()) {
                worker.join();
            }
        }
    }

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    std::condition_variable wait_condition;

    std::atomic<bool> bStopReq{false};
};

template<class F, class... Args>
auto ThreadPool::PushTask_WithFuture(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
{
    using return_type = std::invoke_result_t<F, Args...>;

    // 使用 shared_ptr 包装 packaged_task 使其可拷贝
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        [func = std::forward<F>(f), args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            return std::apply(std::move(func), std::move(args));
        });

    std::future<return_type> res = task->get_future();

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if(bStopReq.load(std::memory_order_acquire)) {
            std::promise<return_type> p;

            // 编译期区分 void 和非 void 类型
            if constexpr(std::is_void_v<return_type>) {
                p.set_value(); // promise<void> 不需要传参
            } else {
                p.set_value(return_type{}); // promise<T> 传入默认构造的值
            }

            return p.get_future();
        }
        tasks.emplace([task]() { (*task)(); });
    }

    condition.notify_one();

    return res;
}

template<class F, class... Args>
void ThreadPool::PushTask(F&& f, Args&&... args)
{
    auto task = [func = std::forward<F>(f), args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
        std::apply(std::move(func), std::move(args));
    };

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (bStopReq.load(std::memory_order_acquire)) return;
        tasks.emplace(std::move(task));
    }

    condition.notify_one();
}


#endif // THREADPOOL_H
