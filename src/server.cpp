#include "server.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "connection.h"
#include "resp_parser.h"
#include "resp_serializer.h"
#include "store.h"
#include "command_handler.h"

Server::Server(int port) : port_(port), server_fd_(-1), command_handler_(store_) {}


Server::~Server() {
    if(server_fd_ != -1) {
        close(server_fd_);
    }
}

void Server::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void Server::initSocket() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if(bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr <<"bind failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    if(listen(server_fd_, 128) < 0) {
        std::cerr << "Listen failed" << std::endl;
        exit(EXIT_FAILURE);
    }
    
    setNonBlocking(server_fd_);

    std::cout << "Server listening on port " << port_ << std::endl;


}

void Server::acceptNewClient() {
    while(true) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        if(client_fd < 0) {
            break;
        }

        setNonBlocking(client_fd);
        event_loop_.addFd(client_fd);
        connections_[client_fd] = std::make_unique<Connection>(client_fd);

        std::cout   << "Client connected: " 
                    << inet_ntoa(client_addr.sin_addr) 
                    << ":" << ntohs(client_addr.sin_port) 
                    << " (fd="
                    << client_fd << ")" << std::endl;



    }
}

void Server::removeClient(int fd) {
    std::cout << "Client disconnected (fd=" << fd <<")" << std::endl;
    event_loop_.removeFd(fd);
    connections_.erase(fd);
    close(fd);
} 

void Server::handleRead(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    Connection& conn = *it->second;

    if (!conn.readFromSocket()) {
        removeClient(fd);
        return;
    }

    processCommands(conn);

    if (conn.hasOutgoingData()) {
        if (!conn.writeToSocket()) {
            removeClient(fd);
            return;
        }
        if (conn.hasOutgoingData()) {
            event_loop_.setWritable(fd, true);
        }
    }
}

void Server::handleWrite(int fd) {
    auto it = connections_.find(fd);
    if(it == connections_.end()) return;

    Connection& conn = *it->second;

    if(!conn.writeToSocket()) {
        removeClient(fd);
        return;
    }

    if(!conn.hasOutgoingData()) {
        event_loop_.setWritable(fd, false);
    }

}

void Server::processCommands(Connection& conn) {
    while (true) {
        RespCommand cmd;
        size_t bytes_consumed = 0;

        ParseResult result = parser_.parse(conn.getReadBuffer(), cmd,
                                            bytes_consumed);

        if (result == ParseResult::INCOMPLETE) {
            break;
        }

        if (result == ParseResult::ERROR) {
            conn.queueWrite("-ERR protocol error\r\n");
            conn.consumeReadBuffer(conn.getReadBuffer().size());
            break;
        }

        conn.consumeReadBuffer(bytes_consumed);

        // Execute the command and get the RESP response
        std::string response = command_handler_.execute(cmd);

        std::cout << "Command: ";
        for (const auto& arg : cmd.args) {
            std::cout << "'" << arg << "' ";
        }
        std::cout << std::endl;

        conn.queueWrite(response);
    }
}


void Server::eventLoop() {
    event_loop_.addFd(server_fd_);

    std::cout << "Event loop started" << std::endl;

    while(true) {
        std::vector<Event> events = event_loop_.poll(-1);

        for(const auto& event : events) {
            if(event.fd == server_fd_) {
                acceptNewClient();
            } else if (event.readable) {
                handleRead(event.fd);
            } else if(event.writable) {
                handleWrite(event.fd);
            }
        }
    }
}



void Server::start() {
    initSocket();
    eventLoop();
}


