#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H
#include <vector>
#include <cstdint>

struct Event {
    int fd;
    bool readable;
    bool writable;
};

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    void addFd(int fd);
    void removeFd(int fd);

    void setWritable(int fd, bool enable);
    std::vector<Event> poll(int timeout_ms = -1);

private:
    int loop_fd_;

};





#endif



