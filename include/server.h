#ifndef SERVER_H
#define SERVER_H

#include "event_loop.h"
#include "resp_parser.h"
#include "connection.h"
#include <memory>
#include <unordered_map>
#include "store.h"
#include "command_handler.h"
#include "thread_pool.h"

class Server{
public:
    Server(int port);
    ~Server();
    void start();

private:
    int port_;
    int server_fd_;
    EventLoop event_loop_;
    RespParser parser_;
    Store store_;
    CommandHandler command_handler_;
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
    ThreadPool thread_pool_;

    void initSocket();
    void setNonBlocking(int fd);
    void eventLoop();
    void acceptNewClient();
    void handleRead(int fd);
    void handleWrite(int fd);
    void removeClient(int fd);
    void processCommands(Connection& conn);


};


#endif

