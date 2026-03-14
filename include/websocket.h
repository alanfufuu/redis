#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <string>
#include <cstdint>
#include <vector>

class WebSocket {
public:
    static std::string computeAcceptKey(const std::string& client_key);

    static std::string handshakeResponse(const std::string& client_key);

    static std::string encodeFrame(const std::string& message);

    static std::string decodeFrame(const std::string& buffer, size_t& bytes_consumed, bool& complete);

    static std::string extractKey(const std::string& request);

    static bool isUpgradeRequest(const std::string& request);
};

#endif