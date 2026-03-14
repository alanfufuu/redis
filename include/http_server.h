#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "event_loop.h"
#include "connection.h"
#include "metrics.h"
#include "store.h"
#include "pg_client.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include "websocket.h"

class HttpServer {
public:
    HttpServer(int port, EventLoop& event_loop);
    ~HttpServer();

    void init();

    void setMetrics(Metrics* metrics);
    void setStore(Store* store);
    void setPgClient(PgClient* pg);

    void acceptNewClient();
    void handleRead(int fd);
    bool ownsfd(int fd) const;
    int getServerFd() const;
    void broadcastMetrics();

private:
    int port_;
    int server_fd_;
    EventLoop& event_loop_;
    Metrics* metrics_;
    Store* store_;
    PgClient* pg_client_;
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
    std::unordered_set<int> ws_clients_; 

    void setNonBlocking(int fd);
    void removeClient(int fd);

    std::string handleRequest(const std::string& raw_request);
    void handleWebSocketFrame(int fd, Connection& conn);

    std::string handleHealth();
    std::string handleStats();
    std::string handleLatency();
    std::string handleHistory();

    std::string httpResponse(int status_code, const std::string& status_text,
                              const std::string& body);
    std::string jsonResponse(const std::string& json);
    std::string notFoundResponse();
};

#endif