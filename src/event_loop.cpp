#include "event_loop.h"
#include <iostream>
#include <unistd.h>
#include <cstring>

#ifdef __APPLE__
    #include <sys/event.h>
#elif __linux__
    #include <sys/epoll.h>
#endif

EventLoop::EventLoop() {
#ifdef __APPLE__
    loop_fd_ = kqueue();
#elif __linux__
    loop_fd_ = epoll.create1(0);
#endif

    if (loop_fd_ == -1) {
        std::cerr << "Failed to create event loop" << std::endl;
        exit(EXIT_FAILURE);
    }
}

EventLoop::~EventLoop() {
    if(loop_fd_ != -1) {
        close(loop_fd_);
    }
}

void EventLoop::addFd(int fd) {
#ifdef __APPLE__
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    if (kevent(loop_fd_, &ev, 1, nullptr, 0, nullptr) == -1) {
        std::cerr << "kqueue: failed to add fd" << std::endl;
    }
#elif __linux__
    struct epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if(epoll_ctl(loop_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        std::cerr << "epoll: failed to add fd" << std::endl;
    }
#endif
}

void EventLoop::removeFd(int fd) {
#ifdef __APPLE__
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    kevent(loop_fd_, &ev, 1, nullptr, 0, nullptr);
    EV_SET(&ev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    kevent(loop_fd_, &ev, 1, nullptr, 0, nullptr);
#elif __linux__
    epoll_ctl(loop_fd_, EPOLL_CTL_DEL, fd, nullptr);
#endif

}

void EventLoop::setWritable(int fd, bool enable) {
#ifdef __APPLE__
    struct kevent ev;
    uint16_t flags = enable ? (EV_ADD | EV_ENABLE) : EV_DELETE;
    EV_SET(&ev, fd, EVFILT_WRITE, flags, 0, 0, nullptr);
    kevent(loop_fd_, &ev, 1, nullptr, 0, nullptr);
#elif __linux__
    struct epoll_event ev{};
    ev.data.fd = fd;
    ev.events = EPOLLIN | (enable ? EPOLLOUT : 0);
    epoll_ctl(loop_fd_, EPOLL_CTL_MOD, fd, &ev);
#endif
}

std::vector<Event> EventLoop::poll(int timeout_ms) {
    std::vector<Event> results;

#ifdef __APPLE__
    struct timespec ts;
    struct timespec* ts_ptr = nullptr;
    if (timeout_ms >= 0) {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;
        ts_ptr = &ts;
    }

    struct kevent events[1024];
    int n = kevent(loop_fd_, nullptr, 0, events, 1024, ts_ptr);

    for (int i = 0; i < n; i++) {
        Event e;
        e.fd       = static_cast<int>(events[i].ident);
        e.readable = (events[i].filter == EVFILT_READ);
        e.writable = (events[i].filter == EVFILT_WRITE);
        results.push_back(e);
    }

#elif __linux__
    struct epoll_event events[1024];
    int n = epoll_wait(loop_fd_, events, 1024, timeout_ms);

    for (int i = 0; i < n; i++) {
        Event e;
        e.fd       = events[i].data.fd;
        e.readable = (events[i].events & EPOLLIN) != 0;
        e.writable = (events[i].events & EPOLLOUT) != 0;
        results.push_back(e);
    }
#endif
    return results;
}


