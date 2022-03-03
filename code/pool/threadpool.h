#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {
            assert(threadCount > 0);

            // 创建threadCount个子线程
            for(size_t i = 0; i < threadCount; i++) {
                /*******************************************************
                2022.1.28
                捕获列表可以用[this],函数体能直接访问类成员，但捕获列表不能是类成员,[pool_]会报错
                此外可以用[pool = pool_]，c++14特性，复制捕获
                ********************************************************/
                std::thread([this] {
                    std::unique_lock<std::mutex> locker(pool_->mtx);
                    while(true) {
                        // LOG_DEBUG("shared_ptr<pool_>.use_count:%d",pool_.use_count());
                        if(!pool_->tasks.empty()) {
                            // 从任务队列中取一个任务
                            auto task = std::move(pool_->tasks.front());
                            // 移除掉
                            pool_->tasks.pop();
                            locker.unlock();
                            task();
                            locker.lock();
                        } 
                        else if(pool_->isClosed) break;
                        else pool_->cond.wait(locker);
                    }
                }).detach();// 线程分离
            }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;
    
    ~ThreadPool() {
        if(static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }
            pool_->cond.notify_all();
        }
    }

    template<class F>
    void AddTask(F&& task) {
        /************************************************************
        2022.1.28
        lock_guard是作用域锁，在构造函数中加锁，离开作用域时，其析构函数释放锁
        std::forward是完美转发，当传入参数task为左值时，转发为左值，反之转发
        为右值，相应调用tasks.emplace中的移动构造函数。若没有这一步，task即使是
        右值引用，但也是左值，将多调用一次拷贝构造函数，影响性能。
        ************************************************************/
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));
            // pool_->tasks.emplace(task);
        }
        pool_->cond.notify_one();
    }

private:
    // 结构体，池子
    struct Pool {
        std::mutex mtx;     // 互斥锁
        std::condition_variable cond;   // 条件变量
        bool isClosed;          // 是否关闭
        std::queue<std::function<void()>> tasks;    // 队列（保存的是任务）
    };
    std::shared_ptr<Pool> pool_;  //  池子
};


#endif //THREADPOOL_H