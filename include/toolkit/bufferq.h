#ifndef BUFFERQ_H
#define BUFFERQ_H

#include <iostream>
#include <condition_variable>
#include <queue>

template <typename T>
class OkQueue
{
public:
    OkQueue(int size) : size(size) {
                            };
    ~OkQueue() {
    };
    void push(T msg)
    {
        std::unique_lock<std::mutex> lock(mtx);
        // 如果缓冲区满了则释放锁并等待
        notFull.wait(lock, [this]()
                     { return queue.size() < size; });
        queue.emplace(msg);
        notEmpty.notify_one();
    };
    T &pop()
    {
        std::unique_lock<std::mutex> lock(mtx);
        // 如果缓冲区为空则释放锁并等待
        notEmpty.wait(lock, [this]()
                      { return queue.size() > 0; });
        auto &msg= queue.front();
        queue.pop();
        notFull.notify_one();
        return msg;
    };

private:
    std::mutex mtx;
    std::condition_variable notFull;
    std::condition_variable notEmpty;
    std::queue<T> queue;
    int size;
};


#endif
