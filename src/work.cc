#include "work.h"

#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <functional>

#include "logger.h"
#include "produce.h"

Work::Work(int listenfd) {
    this->listenfd = listenfd;
    this->event = new Event();
    this->buffer_tree_ = new ReadBufferTree();
}

Work::~Work() { delete this->event; }

void Work::start() {
    this->event->addEvent(this->listenfd, EPOLLIN);

    /* 绑定函数与 epoll 事件 */
    std::function<void(int)> func1, func2, func3, func4;
    func1 = std::bind(&Work::handleRead, this, std::placeholders::_1);
    this->event->setHandleReadFunc(func1);
    func2 = std::bind(&Work::handleWrite, this, std::placeholders::_1);
    this->event->setHandleWriteFunc(func2);
    func3 = std::bind(&Work::handleClose, this, std::placeholders::_1);
    this->event->setHandleCloseFunc(func3);
    func4 = std::bind(&Work::handleError, this, std::placeholders::_1);
    this->event->setHandleErrorFunc(func4);

    this->event->loop();
}

/**
 * 描述符可读的事件
 * */
void Work::handleRead(int fd) {
    /* 判断是否是新的连接 */
    if (fd == this->listenfd) {
        this->handleAccept(fd);
    } else {
        int len;
        ReadBufferNode *node = this->buffer_tree_->Find(fd);
        ReadBuffer *reader = node->reader;
        len = reader->Read();
        if (len == -1) {
            Logger::WARNING("read failure!");
            this->event->deleteEvent(fd, EPOLLIN);
        } else if (reader->End() || len != BUFSIZ) { /* 读取结束的情况 */
            /* 根据请求产生返回 */
            node->produce = new Produce(reader->buffer);
            node->produce->Make();
            node->produce->GetResponse()->MakeHeader();
            this->event->modifyEvent(fd, EPOLLOUT);
        }
    }
}

/**
 * 描述符可写的事件
 * */
void Work::handleWrite(int fd) {
    this->event->deleteEvent(fd, EPOLLOUT);
    ReadBufferNode *node = this->buffer_tree_->Find(fd);
    Response *response = node->produce->GetResponse();
    int n;
    if ((n = write(fd, response->header_buf, response->header_len)) < 0) {
        Logger::WARNING("write failure!");
        close(fd);
        this->buffer_tree_->Remove(fd);
        return;
    }
    if ((n = write(fd, response->buf + response->offset,
                   response->len - response->offset)) < 0) {
        Logger::WARNING("write failure!");
        close(fd);
        this->buffer_tree_->Remove(fd);
        return;
    }
    close(fd);
    this->buffer_tree_->Remove(fd);
}

/**
 * 描述符关闭的事件
 * */
void Work::handleClose(int fd) {
    close(fd);
    this->event->deleteEvent(fd, EPOLLIN);
    this->event->deleteEvent(fd, EPOLLOUT);
}

/**
 * Error 事件
 * */
void Work::handleError(int fd) {
    close(fd);
    this->event->deleteEvent(fd, EPOLLIN);
    this->event->deleteEvent(fd, EPOLLOUT);
}

/**
 * 尝试接收连接并将读取客户端数据的事件添加到 epoll 中
 * */
void Work::handleAccept(int fd) {
    int clientfd;
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen;
    clientfd = accept(listenfd, (struct sockaddr *)&clientAddr, &clientAddrLen);

    if (clientfd == -1) {
        Logger::WARNING("accept failure!");
        return;
    } else {
        Logger::DEBUG("accept a new client: %s:%d",
                      inet_ntoa(clientAddr.sin_addr), clientAddr.sin_port);
        /* 添加描述符监听, 插入 buffer_tree 中 */
        this->event->addEvent(clientfd, EPOLLIN);
        this->buffer_tree_->Insert(clientfd);
    }
}