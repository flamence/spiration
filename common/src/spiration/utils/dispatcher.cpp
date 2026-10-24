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
    std::function<void()> wake;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        ui_tasks_.emplace_back(std::move(fn));
        // 拷贝一份唤醒回调：调用时不能在持锁状态下执行外部代码。
        wake = wake_cb_;
    }
    // 空闲消息循环阻塞等待，必须显式唤醒才能及时消费 UI 任务。
    if (wake) wake();
}

void task_dispatcher::set_wake_callback(std::function<void()> cb) {
    std::lock_guard<std::mutex> lk(mtx_);
    wake_cb_ = std::move(cb);
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
        wake_cb_ = nullptr;
    }
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();
}

} // namespace spiration
