#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <functional>
class ThreadPool_{
public:
    static ThreadPool_& GetThreadPool(size_t num);
    static ThreadPool_* tp;
    static std::once_flag once;
    void stop();
    template<class F,class ...Args>
    void add_task(F &&f,Args &&...args){
        std::function<void()> func=std::bind(std::forward<F>(f),std::forward<Args>(args)...);
        {
            std::lock_guard<std::mutex> lock(mtx);
            task_queue.emplace(func);
        }
        cond_.notify_one();
    }
    ~ThreadPool_();
private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> task_queue;
    std::mutex mtx;
    std::condition_variable cond_;
    ThreadPool_(size_t thread_num);
    ThreadPool_(const ThreadPool_ &t) = delete;
    ThreadPool_& operator = (const ThreadPool_ &t) = delete;
    std::atomic<bool> stop_;
    class PtrDel{
        public:
            PtrDel(){};
            ~PtrDel(){
                if(ThreadPool_::tp){
                    delete tp;
                    ThreadPool_::tp=nullptr;
                }
            }
    };
};