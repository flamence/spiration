/**
 * @file dispatcher.h
 * @brief 任务调度器定义。
 * @author clk
 */

#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace spiration {

/**
 * @brief 轻量任务调度器：后台线程池执行任务，UI 队列回投主线程。
 */
class task_dispatcher {
public:
    /**
     * @brief 构造并启动指定数量的后台工作线程。
     * @param worker_count 工作线程数量
     */
    explicit task_dispatcher(size_t worker_count = 4);

    ~task_dispatcher();

    task_dispatcher(const task_dispatcher&) = delete;
    task_dispatcher& operator=(const task_dispatcher&) = delete;

    /**
     * @brief 在后台线程执行任务。
     * @param fn 待执行任务
     * @return 任务结果句柄
     */
    template <typename Fn>
    std::future<typename std::result_of<Fn()>::type> post(Fn&& fn) {
        using result_type = typename std::result_of<Fn()>::type;
        auto task = std::make_shared<std::packaged_task<result_type()>>(std::forward<Fn>(fn));
        std::future<result_type> fut = task->get_future();
        {
            std::lock_guard<std::mutex> lk(mtx_);
            tasks_.emplace_back([task]() { (*task)(); });
        }
        cv_.notify_one();
        return fut;
    }

    /**
     * @brief 投递任务至 UI 线程队列。
     * @param fn 待执行任务
     */
    void post_to_ui(std::function<void()> fn);

    /**
     * @brief 在 UI 线程消费待执行任务。
     */
    void drain_ui_queue();

    /**
     * @brief 停止后台线程并等待其退出。
     */
    void shutdown();

private:
    void worker_loop();

    std::vector<std::thread> workers_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> tasks_;
    std::deque<std::function<void()>> ui_tasks_;
    bool stop_ = false;
};

} // namespace spiration
