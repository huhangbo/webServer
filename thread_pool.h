#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <exception>
#include <list>
#include <cstdio>
#include "locker.h"
#include "sem.h"

template<typename T>
class thread_pool {
public:
    explicit thread_pool(int thread_num = 8, int max_requests = 1000) :
    m_thread_num(thread_num), m_max_requests(max_requests), m_stop(false), m_threads(nullptr) {
        if((thread_num <= 0 || max_requests <= 0)) {
            throw std::exception();
        }
        m_threads = new pthread_t[thread_num];
        if(!thread_num) {
            throw std::exception();
        }
        for (int i = 0; i < thread_num; i++) {
            printf("creat the %dth thread, %d", i);
            if(pthread_create(m_threads + i, nullptr, worker, this) != 0) {
                delete []m_threads;
                throw std::exception();
            }
            if(pthread_detach(m_threads[i])) {
                throw std::exception();
            }
        }
    }

    ~thread_pool() {
        delete []m_threads;
        m_stop = true;
    }

    bool append(T* request) {
        m_queue_locker.lock();
        if(m_work_queue.size() > m_max_requests) {
            m_queue_locker.unlock();
            return false;
        }
        m_work_queue.push_back(request);
        m_queue_locker.unlock();
        m_queue_stat.post();
        return true;
    }

private:
    int m_thread_num;

    pthread_t* m_threads;

    int m_max_requests;

    std::list<T*> m_work_queue;

    locker m_queue_locker;

    sem m_queue_stat;

    bool m_stop;

    static void* worker(void* arg) {
        auto* pool = (thread_pool*) arg;
        pool->run();
        return pool;
    }

    void run() {
        while (!m_stop) {
            m_queue_stat.wait();
            m_queue_locker.lock();
            if(m_work_queue.empty()) {
                m_queue_locker.unlock();
                continue;
            }

            auto request = m_work_queue.front();
            m_work_queue.pop_front();
            m_queue_locker.unlock();

            if(!request) {
                continue;
            }

            request->process();
        }
    }

};


#endif
