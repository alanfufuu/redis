#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();

    void submit(std::function<void()> task);

    size_t pendingTasks() const;

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> task_queue_;

    mutable std::mutex mutex_;

    std::condition_variable cv_;
    bool shutdown_;
    void workerLoop();
};

#endif