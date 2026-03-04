#ifndef CONNECTION_H
#define CONNECTION_H

#include <string>

// read and write buffers

class Connection {
public:
    explicit Connection(int fd);
    int getFd() const;

    // pull bytes from socket into read buffer, return false if client disconnected or error
    bool readFromSocket();
    //pull bytes from write buffer into socket, return false if client disconnected or error
    bool writeToSocket();
    void queueWrite(const std::string &data);
    bool hasOutgoingData() const;
    const std::string& getReadBuffer() const;
    void consumeReadBuffer(size_t bytes);

private:
    int fd_;
    std::string read_buffer_;
    std::string write_buffer_;



};





#endif