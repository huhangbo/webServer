#ifndef COND_H
#define COND_H

#include <pthread.h>
#include <exception>

class cond {
public:
    cond() {
        if(pthread_cond_init(&m_cond, nullptr) != 0) {
            throw std::exception();
        }
    }

    ~cond() {
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t * mutex) {
        return pthread_cond_wait(&m_cond, mutex) == 0;
    }

    bool timeWait(pthread_mutex_t * mutex, timespec t) {
        return pthread_cond_timedwait(&m_cond, mutex, &t);
    }

    bool signal() {
        return pthread_cond_signal(&m_cond) == 0;
    }

    bool broadcast() {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    pthread_cond_t m_cond{};
};

#endif
