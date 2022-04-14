#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cerrno>
#include <csignal>
#include "thread_pool.h"
#include "http_conn.h"


#define MAX_FD 655335
#define MAX_EVENT_NUM 10000

void addSign(int sig, void(*handler)(int i)) {
    struct sigaction sa{};
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, nullptr);
}


int main(int argc, char* argv[]) {
    if(argc <= 1) {
        printf("按照如下格式运行，port num %s\n", basename(argv[0]));
        exit(-1);
    }

    int port = atoi(argv[1]);

    addSign(SIGPIPE, SIG_IGN);

    thread_pool<http_conn>* pool;
    try {
        pool = new(thread_pool<http_conn>);
    }catch(...){
        exit(-1);
    }

    http_conn* users = new http_conn[MAX_FD];

    int listenFd = socket(PF_INET, SOCK_STREAM, 0);

    int reUse = 1;

    setsockopt(listenFd, SOL_SOCKET, SO_REUSEPORT, &reUse, sizeof(reUse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    bind(listenFd, (sockaddr*)(&address), sizeof(address));

    listen(listenFd, 5);

    epoll_event events[MAX_EVENT_NUM];

    int epollFd = epoll_create(5);

    http_conn::addFd(epollFd, listenFd, true);

    http_conn::m_epoll_fd = epollFd;

    while (true) {
        int num = epoll_wait(epollFd, events, MAX_EVENT_NUM, -1);
        if(num < 0 && errno != EINTR) {
            perror("epoll");
            break;
        }

        for (int i = 0; i < num; ++i) {
            int sockFd = events[i].data.fd;
            if(sockFd == listenFd) {
                sockaddr_in client_address{};
                socklen_t sockLen = sizeof(client_address);
                int connFd = accept(listenFd, (sockaddr*)&client_address, &sockLen);
                if(http_conn::m_user_count > MAX_FD) {
                    //todo 添加忙碌信息
                    close(connFd);
                    continue;
                }
                users[connFd].init(connFd, client_address);
            } else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                users[sockFd].closeConn();
            } else if(events[i].events & EPOLLIN) {
                if(users[sockFd].read()) {
                    pool->append(users + sockFd);
                } else {
                    users[sockFd].closeConn();
                }
            } else if(events[i].events & EPOLLOUT) {
                if (!users[sockFd].write()) {
                    users[sockFd].closeConn();
                }
            }
        }
        
    }

    close(epollFd);
    close(listenFd);
    delete [] users;
    delete pool;

    return 0;
}
