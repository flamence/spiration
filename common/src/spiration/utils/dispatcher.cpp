/**
 * @file dispatcher.cpp
 * @brief 任务调度器实现。
 * @author clk
 */

#include <spiration/utils/dispatcher.h>

namespace spiration {

task_dispatcher::task_dispatcher(size_t worker_count) {
    workers_.reserve(worker_count);
    for (size_t i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this]() { worker_loop(); });
    }
}

task_dispatcher::~task_dispatcher() {
    shutdown();
}

void task_dispatcher::worker_loop() {
    std::unique_lock<std::mutex> lk(mtx_);
    while (true) {
        cv_.wait(lk, [this]() { return stop_ || !tasks_.empty(); });
        if (stop_ && tasks_.empty()) return;
        std::function<void()> fn = std::move(tasks_.front());
        tasks_.pop_front();
        lk.unlock();
        fn();
        lk.lock();
    }
}

void task_dispatcher::post_to_ui(std::function<void()> fn) {
    std::lock_guard<std::mutex> lk(mtx_);
    ui_tasks_.emplace_back(std::move(fn));
}

void task_dispatcher::drain_ui_queue() {
    std::deque<std::function<void()>> pending;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        pending.swap(ui_tasks_);
    }
    for (auto& fn : pending) {
        fn();
    }
}

void task_dispatcher::shutdown() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (stop_) return;
        stop_ = true;
        tasks_.clear();
        ui_tasks_.clear();
    }
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();
}

} // namespace spiration
