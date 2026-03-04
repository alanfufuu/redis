#include "connection.h"
#include <sys/socket.h>
#include <cerrno>
#include <iostream>


Connection::Connection(int fd) : fd_(fd) {}

int Connection::getFd() const {
    return fd_;
}

bool Connection::readFromSocket() {
    char buffer[4096];

    while(true) {
        int bytes_read = recv(fd_, buffer, sizeof(buffer), 0);

        if (bytes_read > 0) {
            read_buffer_.append(buffer, bytes_read);
        } else if (bytes_read == 0) {
            return false;
        } else {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            return false;
        }
    }
    return true;
}

bool Connection::writeToSocket() {
    while(!write_buffer_.empty()) {
        int bytes_sent = send(fd_, write_buffer_.data(), write_buffer_.size(), 0);

        if (bytes_sent > 0) {
            write_buffer_.erase(0, bytes_sent);

        } else if(bytes_sent == -1) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return false;
        }
        
    }
    return true;
}

void Connection::queueWrite(const std::string& data) {
    write_buffer_.append(data);
}

bool Connection::hasOutgoingData() const {
    return !write_buffer_.empty();
}

const std::string& Connection::getReadBuffer() const {
    return read_buffer_;
}

void Connection::consumeReadBuffer(size_t bytes) {
    read_buffer_.erase(0, bytes);
}






