#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <string>
#include <cstdint>
#include <vector>

class WebSocket {
public:
    // Generate the Sec-WebSocket-Accept value for the handshake
    static std::string computeAcceptKey(const std::string& client_key);

    // Build the HTTP 101 handshake response
    static std::string handshakeResponse(const std::string& client_key);

    // Encode a message into a WebSocket frame
    static std::string encodeFrame(const std::string& message);

    // Decode a WebSocket frame from raw bytes.
    // Returns the payload, or empty string if incomplete.
    // Sets `bytes_consumed` to how many bytes were used.
    static std::string decodeFrame(const std::string& buffer,
                                    size_t& bytes_consumed, bool& complete);

    // Extract the Sec-WebSocket-Key from an HTTP upgrade request
    static std::string extractKey(const std::string& request);

    // Check if an HTTP request is a WebSocket upgrade
    static bool isUpgradeRequest(const std::string& request);
};

#endif