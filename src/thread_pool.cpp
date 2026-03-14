#include "thread_pool.h"
#include <iostream>

ThreadPool::ThreadPool(size_t num_threads) : shutdown_(false) {
    for (size_t i = 0; i < num_threads; i++) {
        workers_.emplace_back(&ThreadPool::workerLoop, this);
    }
    std::cout << "Thread pool started with " << num_threads << " workers" << std::endl;
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    cv_.notify_all();


    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    std::cout << "Thread pool shut down" << std::endl;
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        task_queue_.push(std::move(task));
    }
    cv_.notify_one();
}

size_t ThreadPool::pendingTasks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return task_queue_.size();
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;

        {

            std::unique_lock<std::mutex> lock(mutex_);

            cv_.wait(lock, [this] {
                return !task_queue_.empty() || shutdown_;
            });

            if (shutdown_ && task_queue_.empty()) {
                return;
            }

            task = std::move(task_queue_.front());
            task_queue_.pop();

        }

        try {
            task();
        } catch (const std::exception& e) {
            std::cerr << "Thread pool task threw exception: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Thread pool task threw unknown exception" << std::endl;
        }
    }
}