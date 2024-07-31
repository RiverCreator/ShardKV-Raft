#include "ThreadPool.h"
ThreadPool_ *ThreadPool_::tp;
std::once_flag ThreadPool_::once;
ThreadPool_& ThreadPool_::GetThreadPool(size_t num)
{
    std::call_once(once,[&](){
        if(!tp){
            tp=new ThreadPool_(num);
            static PtrDel ptr;
        }
    });
    return *tp;
}

ThreadPool_::ThreadPool_(size_t thread_num)
{
    for(int i=0;i<thread_num;i++){
        threads_.emplace_back([this](){
            while(1){
                std::function<void()> fn;
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    cond_.wait(lock,[this](){
                        return this->stop_.load()||!this->task_queue.empty();
                    });
                    if(this->stop_&&this->task_queue.empty())
                        return;
                    fn=std::move(this->task_queue.front());
                    this->task_queue.pop();
                }
                fn();
            }
        });
    }
}

void ThreadPool_::stop(){
    if(this->stop_){
        return;
    }
    this->stop_=true;
    cond_.notify_all();
    for(auto& th:threads_){
        th.join();
    }
}

ThreadPool_::~ThreadPool_(){
    stop();
}