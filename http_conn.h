#ifndef WEBSERVER_HTTP_CONN_H
#define WEBSERVER_HTTP_CONN_H

#include <sys/epoll.h>

void setNoBlocking(int fd) {
    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd, new_flag);
}


class http_conn {
public:

    static int m_epoll_fd;
    static int m_user_count;

    http_conn() {

    }
    ~http_conn() {

    }

    void init(int sockFd, const sockaddr_in& address ) {
        m_sock_fd = sockFd;
        m_address = address;

        int reUse = 1;
        setsockopt(m_sock_fd, SOL_SOCKET, SO_REUSEPORT, &reUse, sizeof(reUse));
        addFd(m_epoll_fd, sockFd, true);
        m_user_count++;
    }

    void addFd(int epollFd, int fd, bool one_shot) {
        epoll_event event{};
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLRDHUP;
        if(one_shot) {
            event.events | EPOLLONESHOT;
        }

        epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);
        setNoBlocking(fd);
    }


    void removeFd(int epollFd, int fd) {
        epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
    }

    void modifyFd(int epollFd, int fd, int ev) {
        epoll_event event{};
        event.data.fd = fd;
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

        epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &event);
    }

    void process() {
        printf("解析请求，返回结果");
    }

    void closeConn() {
        if(m_sock_fd != -1) {
            removeFd(m_epoll_fd, m_sock_fd);
            m_sock_fd = -1;
            m_user_count--;
        }
    }

    bool read() {
        printf("一次性读数据\n");
        return true;
    }

    bool write() {
        printf("一次性写数据\n");
        return true;
    }

private:
    int m_sock_fd;
    sockaddr_in m_address;
};



#endif //WEBSERVER_HTTP_CONN_H
