#include "http_server.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sstream>
#include <iomanip>

HttpServer::HttpServer(int port, EventLoop& event_loop)
    : port_(port),
      server_fd_(-1),
      event_loop_(event_loop),
      metrics_(nullptr),
      store_(nullptr),
      pg_client_(nullptr) {}

HttpServer::~HttpServer() {
    if (server_fd_ != -1) {
        close(server_fd_);
    }
    for (auto& [fd, conn] : connections_) {
        close(fd);
    }
}

void HttpServer::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void HttpServer::setMetrics(Metrics* metrics) { metrics_ = metrics; }
void HttpServer::setStore(Store* store) { store_ = store; }
void HttpServer::setPgClient(PgClient* pg) { pg_client_ = pg; }

void HttpServer::init() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == -1) {
        std::cerr << "HTTP: Failed to create socket" << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "HTTP: Bind failed on port " << port_ << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return;
    }

    if (listen(server_fd_, 64) < 0) {
        std::cerr << "HTTP: Listen failed" << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return;
    }

    setNonBlocking(server_fd_);
    event_loop_.addFd(server_fd_);

    std::cout << "HTTP API listening on port " << port_ << std::endl;
}

int HttpServer::getServerFd() const {
    return server_fd_;
}

bool HttpServer::ownsfd(int fd) const {
    if (fd == server_fd_) return true;
    return connections_.find(fd) != connections_.end();
}

void HttpServer::acceptNewClient() {
    while (true) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) break;

        setNonBlocking(client_fd);
        event_loop_.addFd(client_fd);
        connections_[client_fd] = std::make_unique<Connection>(client_fd);
    }
}

void HttpServer::removeClient(int fd) {
    ws_clients_.erase(fd);
    event_loop_.removeFd(fd);
    connections_.erase(fd);
    close(fd);
}


std::string HttpServer::httpResponse(int status_code, const std::string& status_text,
                                      const std::string& body) {
    std::ostringstream resp;
    resp << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    resp << "Content-Type: application/json\r\n";
    resp << "Content-Length: " << body.size() << "\r\n";
    resp << "Access-Control-Allow-Origin: *\r\n";
    resp << "Access-Control-Allow-Methods: GET, OPTIONS\r\n";
    resp << "Access-Control-Allow-Headers: Content-Type\r\n";
    resp << "Connection: close\r\n";
    resp << "\r\n";
    resp << body;
    return resp.str();
}

std::string HttpServer::jsonResponse(const std::string& json) {
    return httpResponse(200, "OK", json);
}

std::string HttpServer::notFoundResponse() {
    return httpResponse(404, "Not Found", "{\"error\":\"not found\"}");
}


void HttpServer::handleRead(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    Connection& conn = *it->second;

    if (!conn.readFromSocket()) {
        removeClient(fd);
        return;
    }

    if (ws_clients_.count(fd)) {
        handleWebSocketFrame(fd, conn);
        return;
    }

    const std::string& buffer = conn.getReadBuffer();
    if (buffer.find("\r\n\r\n") == std::string::npos) {
        return;
    }

    std::cout << "HTTP request received:\n" << buffer.substr(0, 200) << std::endl;
    std::cout << "Is upgrade? " << WebSocket::isUpgradeRequest(buffer) << std::endl;

    if (WebSocket::isUpgradeRequest(buffer)) {
        std::string key = WebSocket::extractKey(buffer);
        std::cout << "WebSocket key: " << key << std::endl;
        if (!key.empty()) {
            std::string response = WebSocket::handshakeResponse(key);
            conn.queueWrite(response);
            conn.writeToSocket();
            conn.consumeReadBuffer(buffer.size());
            ws_clients_.insert(fd);
            std::cout << "WebSocket client connected (fd=" << fd << ")" << std::endl;
            return;
        }
    }

    std::string response = handleRequest(buffer);
    conn.queueWrite(response);
    conn.writeToSocket();
    removeClient(fd);
}

std::string HttpServer::handleRequest(const std::string& raw_request) {

    size_t first_space = raw_request.find(' ');
    if (first_space == std::string::npos) return notFoundResponse();

    size_t second_space = raw_request.find(' ', first_space + 1);
    if (second_space == std::string::npos) return notFoundResponse();

    std::string method = raw_request.substr(0, first_space);
    std::string path = raw_request.substr(first_space + 1,
                                           second_space - first_space - 1);

    if (method == "OPTIONS") {
        return httpResponse(204, "No Content", "");
    }

    if (method != "GET") {
        return httpResponse(405, "Method Not Allowed",
                             "{\"error\":\"method not allowed\"}");
    }

    if (path == "/health")       return jsonResponse(handleHealth());
    if (path == "/api/stats")    return jsonResponse(handleStats());
    if (path == "/api/latency")  return jsonResponse(handleLatency());
    if (path == "/api/history")  return jsonResponse(handleHistory());

    return notFoundResponse();
}


std::string HttpServer::handleHealth() {
    return "{\"status\":\"ok\"}";
}

std::string HttpServer::handleStats() {
    if (!metrics_ || !store_) {
        return "{\"error\":\"metrics not available\"}";
    }

    std::ostringstream json;
    json << "{";
    json << "\"active_connections\":" << metrics_->getActiveConnections() << ",";
    json << "\"total_requests\":" << metrics_->getTotalRequests() << ",";
    json << "\"total_keys\":" << store_->size() << ",";
    json << "\"requests_per_second\":" << std::fixed << std::setprecision(2)
         << metrics_->getCurrentRps();
    json << "}";
    return json.str();
}

std::string HttpServer::handleLatency() {
    if (!metrics_) {
        return "{\"error\":\"metrics not available\"}";
    }

    int64_t total_keys = store_ ? static_cast<int64_t>(store_->size()) : 0;
    auto snap = metrics_->takeSnapshot(total_keys);

    std::ostringstream json;
    json << std::fixed << std::setprecision(2);
    json << "{";
    json << "\"avg_latency_us\":" << snap.avg_latency_us << ",";
    json << "\"p50_latency_us\":" << snap.p50_latency_us << ",";
    json << "\"p95_latency_us\":" << snap.p95_latency_us << ",";
    json << "\"p99_latency_us\":" << snap.p99_latency_us << ",";
    json << "\"requests_per_second\":" << snap.requests_per_second << ",";
    json << "\"sample_size\":" << snap.requests.size();
    json << "}";
    return json.str();
}

std::string HttpServer::handleHistory() {
    if (!pg_client_ || !pg_client_->isConnected()) {
        return "{\"error\":\"database not available\",\"snapshots\":[]}";
    }

    PGresult* res = PQexec(pg_client_->getConnection(),
        "SELECT "
        "  EXTRACT(EPOCH FROM recorded_at)::bigint AS ts, "
        "  requests_per_second, "
        "  p50_latency_us, "
        "  p95_latency_us, "
        "  p99_latency_us, "
        "  active_connections, "
        "  total_keys "
        "FROM server_snapshots "
        "ORDER BY recorded_at DESC "
        "LIMIT 60"
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "HTTP: History query failed: " << PQerrorMessage(pg_client_->getConnection())
                  << std::endl;
        PQclear(res);
        return "{\"error\":\"query failed\",\"snapshots\":[]}";
    }

    int rows = PQntuples(res);
    std::ostringstream json;
    json << std::fixed << std::setprecision(2);
    json << "{\"snapshots\":[";

    for (int i = 0; i < rows; i++) {
        if (i > 0) json << ",";
        json << "{";
        json << "\"timestamp\":" << PQgetvalue(res, i, 0) << ",";
        json << "\"rps\":" << PQgetvalue(res, i, 1) << ",";
        json << "\"p50\":" << PQgetvalue(res, i, 2) << ",";
        json << "\"p95\":" << PQgetvalue(res, i, 3) << ",";
        json << "\"p99\":" << PQgetvalue(res, i, 4) << ",";
        json << "\"connections\":" << PQgetvalue(res, i, 5) << ",";
        json << "\"keys\":" << PQgetvalue(res, i, 6);
        json << "}";
    }

    json << "]}";
    PQclear(res);
    return json.str();
}

void HttpServer::handleWebSocketFrame(int fd, Connection& conn) {
    while (true) {
        size_t bytes_consumed = 0;
        bool complete = false;

        std::string payload = WebSocket::decodeFrame(conn.getReadBuffer(),
                                                      bytes_consumed, complete);

        if (!complete) break;

        conn.consumeReadBuffer(bytes_consumed);

        if (payload.empty()) {
            std::cout << "WebSocket client disconnected (fd=" << fd << ")" << std::endl;
            removeClient(fd);
            return;
        }


    }
}


void HttpServer::broadcastMetrics() {
    if (ws_clients_.empty() || !metrics_ || !store_) return;

    std::ostringstream json;
    json << std::fixed << std::setprecision(2);
    json << "{";
    json << "\"type\":\"metrics\",";
    json << "\"active_connections\":" << metrics_->getActiveConnections() << ",";
    json << "\"total_requests\":" << metrics_->getTotalRequests() << ",";
    json << "\"total_keys\":" << store_->size() << ",";
    json << "\"requests_per_second\":" << metrics_->getCurrentRps();
    json << "}";

    std::string frame = WebSocket::encodeFrame(json.str());

    std::vector<int> dead_clients;
    for (int fd : ws_clients_) {
        auto it = connections_.find(fd);
        if (it == connections_.end()) {
            dead_clients.push_back(fd);
            continue;
        }

        Connection& conn = *it->second;
        conn.queueWrite(frame);
        if (!conn.writeToSocket()) {
            dead_clients.push_back(fd);
        }
    }

    for (int fd : dead_clients) {
        removeClient(fd);
    }
}

