#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool {
private:
    // Need to keep track of threads so we can join them
    std::vector<std::thread> workers;
    // The task queue
    std::queue<std::function<void()>> tasks;
    
    // Synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
    
public:
    ThreadPool(size_t threads = 0) : stop(false) {
        // Default to hardware concurrency if 0 is passed
        if (threads == 0) {
            threads = std::thread::hardware_concurrency();
        }
        
        for(size_t i = 0; i < threads; ++i)
            workers.emplace_back(
                [this]
                {
                    for(;;)
                    {
                        std::function<void()> task;
                        
                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            this->condition.wait(lock,
                                [this]{ return this->stop || !this->tasks.empty(); });
                            if(this->stop && this->tasks.empty())
                                return;
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }
                        
                        task();
                    }
                }
            );
    }
    
    // Add new work item to the pool
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type> {
        using return_type = typename std::result_of<F(Args...)>::type;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );
            
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            
            // don't allow enqueueing after stopping the pool
            if(stop)
                throw std::runtime_error("enqueue on stopped ThreadPool");
            
            tasks.emplace([task](){ (*task)(); });
        }
        condition.notify_one();
        return res;
    }
    
    // Wait for all tasks to complete
    void waitForCompletion() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (tasks.empty()) return;
        
        condition.wait(lock, [this]{ return tasks.empty(); });
    }
    
    // Destructor joins all threads
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for(std::thread &worker: workers)
            worker.join();
    }
};

#endif // THREAD_POOL_HPP