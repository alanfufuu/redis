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
        // Lock, set shutdown flag, unlock
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    // Wake ALL workers so they can see the shutdown flag and exit
    cv_.notify_all();

    // Wait for all workers to finish their current task and exit
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
    // Wake one sleeping worker to handle the task
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
            // Acquire the lock
            std::unique_lock<std::mutex> lock(mutex_);

            // Wait until: there's a task in the queue OR shutdown was requested
            // The lambda is the "predicate" — wait() only returns when it's true.
            // This guards against spurious wakeups (OS can wake threads randomly).
            cv_.wait(lock, [this] {
                return !task_queue_.empty() || shutdown_;
            });

            // If shutting down AND no more tasks, exit
            if (shutdown_ && task_queue_.empty()) {
                return;
            }

            // Pop the next task
            task = std::move(task_queue_.front());
            task_queue_.pop();

            // Lock is released here when `lock` goes out of scope
        }

        // Execute the task WITHOUT holding the lock
        // This is critical — if we held the lock during execution,
        // no other worker could pick up tasks, defeating the purpose.
        try {
            task();
        } catch (const std::exception& e) {
            std::cerr << "Thread pool task threw exception: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Thread pool task threw unknown exception" << std::endl;
        }
    }
}