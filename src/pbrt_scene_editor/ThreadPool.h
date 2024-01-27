//
// Created by 王泽远 on 2024/1/27.
//

#ifndef PBRTEDITOR_THREADPOOL_H
#define PBRTEDITOR_THREADPOOL_H
#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads) : running(true) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back(
                    [this, i] {
                        for (;;) {
                            std::function<void(int)> task;

                            {
                                std::unique_lock<std::mutex> lock(this->queue_mutex);
                                this->condition.wait(lock,
                                                     [this] { return !this->running || !this->tasks.empty(); });

                                if (!this->running && this->tasks.empty()) {
                                    return;
                                }

                                task = std::move(this->tasks.front());
                                this->tasks.pop();
                            }
                            task(i);
                        }
                    }
            );
        }
    }

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(int, Args...)>::type> {
        using return_type = typename std::result_of<F(int, Args...)>::type;

        auto task = std::make_shared<std::packaged_task<return_type(int)>>(
                std::bind(std::forward<F>(f), std::placeholders::_1, std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            if (!running) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            tasks.emplace([task](int id) { (*task)(id); });
        }
        condition.notify_one();
        return res;
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            running = false;
        }
        condition.notify_all();
        for (std::thread &worker : workers) {
            worker.join();
        }
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void(int)>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool running;
};

#endif //PBRTEDITOR_THREADPOOL_H
