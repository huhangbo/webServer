#ifndef WEBSERVER_HTTP_CONN_H
#define WEBSERVER_HTTP_CONN_H

#include <sys/epoll.h>

void setNoBlocking(int fd) {
    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd, new_flag);
}

enum METHOD {
    GET = 0,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    OPTIONS,
    CONNECT
};

enum CHECK_STATE {
    CHECK_STATE_REQUESTLINE = 0,    //分析请求行
    CHECK_STATE_HEADER,         //分析头部字段
    CHECK_STATE_CONTENT         //解析请求体
};

enum LINE_STATE {
    LINE_OK = 0,  //完整行
    LINE_BAD,     //行出错
    LINE_OPEN,    //行不完整
};

enum HTTP_CODE {
    NO_REQUEST,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION,
};

class http_conn {
public:

    static int m_epoll_fd;
    static int m_user_count;

    static const int READ_BUF_SIZE = 2048;
    static const int WRITE_BUF_SIZE = 1024;


    http_conn() {

    }
    ~http_conn() {

    }

    HTTP_CODE process_read() {
        LINE_STATE lineState = LINE_OK;
        HTTP_CODE ret = NO_REQUEST;

        char* text = nullptr;
        while ((m_check_state == CHECK_STATE_CONTENT && lineState == LINE_OK)
               || (lineState = parse_line()) == LINE_OK) {
            text = get_line();
            m_start_line = m_check_index;
            printf("get 1 http line: %s \n", text);
            switch (m_check_state) {
                case CHECK_STATE_REQUESTLINE: {
                    ret = parse_request_line(text);
                    if(ret == BAD_REQUEST) {
                        return BAD_REQUEST;
                    }
                    break;
                }
                case CHECK_STATE_HEADER: {
                    ret = parse_request_line(text);
                    if(ret == BAD_REQUEST) {
                        return BAD_REQUEST;
                    } else if(ret == GET_REQUEST) {
                        return do_request();
                    }
                    break;
                }
                case CHECK_STATE_CONTENT: {
                    ret = parse_request_line(text);
                    if(ret == GET_REQUEST) {
                        return do_request();
                    }
                    lineState = LINE_OPEN;
                    break;
                }
                default: {
                    return INTERNAL_ERROR;
                }
            }
        }
        return NO_REQUEST;
    }

    HTTP_CODE parse_request_line(char* text) {
        m_url = strpbrk(text, " \t");
        *m_url++ = '\0';
        char* method = text;
        if(strcasecmp(method, "GET") == 0) {
            m_method = GET;
        } else {
            return BAD_REQUEST;
        }
        m_version = strpbrk(m_url, " \t");
        if(!m_version) {
            return BAD_REQUEST;
        }
        *m_version++ = '\0';
        if(strcasecmp(m_version, "HTTP/1.1") !=0 ) {
            return BAD_REQUEST;
        }
        if(strncasecmp(m_url, "http://", 7) == 0) {
            m_url += 7;
            m_url = strchr(m_url, '/');

        }
        if(!m_url || m_url[0] != '/') {
            return BAD_REQUEST;
        }

        m_check_state = CHECK_STATE_HEADER;
        return NO_REQUEST;
    }

    HTTP_CODE parse_headers(char* text) {

    }

    HTTP_CODE parse_content(char* text) {

    }

    LINE_STATE parse_line() {
        char tmp;
        for(; m_check_index < m_read_index; m_check_index++) {
            tmp = m_read_buf[m_check_index];
            if(tmp == '\r') {
                if((m_check_index + 1) == m_read_index) {
                    return LINE_OPEN;
                } else if(m_read_buf[m_check_index + 1] == '\n') {
                    m_read_buf[m_check_index++] = '\0';
                    m_read_buf[m_check_index++] = '\0';
                    return LINE_OK;
                }
                return LINE_BAD;
            } else if(tmp == '\n') {
                if(m_check_index > 1 && m_read_buf[m_check_index - 1] == '\r') {
                    m_read_buf[m_check_index-1] = '\0';
                    m_read_buf[m_check_index+1] = '\0';
                    return LINE_OK;
                }
                return LINE_BAD;
            }
        }

        return LINE_OK;
    }

    char* get_line() {
        return m_read_buf + m_start_line;
    }

    void init(int sockFd, const sockaddr_in& address ) {
        m_sock_fd = sockFd;
        m_address = address;

        int reUse = 1;
        setsockopt(m_sock_fd, SOL_SOCKET, SO_REUSEPORT, &reUse, sizeof(reUse));
        addFd(m_epoll_fd, sockFd, true);
        m_user_count++;
        init();
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
        HTTP_CODE read_ret = process_read();
        if(read_ret == NO_REQUEST) {
            modifyFd(m_epoll_fd, m_sock_fd, EPOLLIN);
            return;
        }
    }

    void closeConn() {
        if(m_sock_fd != -1) {
            removeFd(m_epoll_fd, m_sock_fd);
            m_sock_fd = -1;
            m_user_count--;
        }
    }

    bool read() {
        if(m_read_index > READ_BUF_SIZE) {
            return false;
        }
        ssize_t bytes_read = 0;
        while(true) {
            bytes_read = recv(m_sock_fd, m_read_buf + m_read_index, READ_BUF_SIZE - m_read_index, 0);
            if(bytes_read == -1) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
            } else if(bytes_read == 0){
                return false;
            }
        }
        m_read_index += int(bytes_read);
        printf("读取到数据：%s\n", m_read_buf);
        return true;
    }

    bool write() {
        printf("一次性写数据\n");
        return true;
    }

    HTTP_CODE do_request() {

    }

private:
    int m_sock_fd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUF_SIZE];
    int m_read_index;

    int m_check_index;
    int m_start_line;
    CHECK_STATE m_check_state;

    void init() {
        m_check_state = CHECK_STATE_REQUESTLINE;
        m_check_index = 0;
        m_start_line = 0;
        m_read_index = 0;
        m_url = nullptr;
        m_method = GET;
        m_version = nullptr;
        bzero(m_read_buf, READ_BUF_SIZE);
        m_linger = false;
    }

    char* m_url;
    char* m_version;
    METHOD m_method;
    char* m_host;
    bool m_linger;   //HTTP请求是否要保持连接

};



#endif //WEBSERVER_HTTP_CONN_H
