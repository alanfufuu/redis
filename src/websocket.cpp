#include "websocket.h"
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <cstring>
#include <sstream>
#include <algorithm>

static const std::string WS_MAGIC_GUID = "258EAFA5-E914-47DA-95CA-5AB5DC11650A";

static std::string base64Encode(const unsigned char* data, size_t len) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);

    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data, static_cast<int>(len));
    BIO_flush(b64);

    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64, &bptr);

    std::string result(bptr->data, bptr->length);
    BIO_free_all(b64);
    return result;
}

std::string WebSocket::computeAcceptKey(const std::string& client_key) {
    std::string combined = client_key + WS_MAGIC_GUID;

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()),
         combined.size(), hash);

    return base64Encode(hash, SHA_DIGEST_LENGTH);
}

std::string WebSocket::handshakeResponse(const std::string& client_key) {
    std::string accept_key = computeAcceptKey(client_key);

    std::ostringstream resp;
    resp << "HTTP/1.1 101 Switching Protocols\r\n";
    resp << "Upgrade: websocket\r\n";
    resp << "Connection: Upgrade\r\n";
    resp << "Sec-WebSocket-Accept: " << accept_key << "\r\n";
    resp << "\r\n";
    return resp.str();
}

std::string WebSocket::encodeFrame(const std::string& message) {
    std::string frame;
    size_t len = message.size();

    frame += static_cast<char>(0x81);

    if (len <= 125) {
        frame += static_cast<char>(len);
    } else if (len <= 65535) {
        frame += static_cast<char>(126);
        frame += static_cast<char>((len >> 8) & 0xFF);
        frame += static_cast<char>(len & 0xFF);
    } else {
        frame += static_cast<char>(127);
        for (int i = 7; i >= 0; i--) {
            frame += static_cast<char>((len >> (i * 8)) & 0xFF);
        }
    }

    frame += message;
    return frame;
}

std::string WebSocket::decodeFrame(const std::string& buffer,
                                    size_t& bytes_consumed, bool& complete) {
    complete = false;
    bytes_consumed = 0;

    if (buffer.size() < 2) return "";

    uint8_t first_byte = static_cast<uint8_t>(buffer[0]);
    uint8_t second_byte = static_cast<uint8_t>(buffer[1]);

    uint8_t opcode = first_byte & 0x0F;
    if (opcode == 0x8) {
        complete = true;
        bytes_consumed = buffer.size();
        return "";  
    }

    bool masked = (second_byte & 0x80) != 0;
    uint64_t payload_len = second_byte & 0x7F;

    size_t header_size = 2;
    if (payload_len == 126) {
        if (buffer.size() < 4) return "";
        payload_len = (static_cast<uint8_t>(buffer[2]) << 8) |
                       static_cast<uint8_t>(buffer[3]);
        header_size = 4;
    } else if (payload_len == 127) {
        if (buffer.size() < 10) return "";
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | static_cast<uint8_t>(buffer[2 + i]);
        }
        header_size = 10;
    }

    size_t mask_size = masked ? 4 : 0;
    size_t total_size = header_size + mask_size + payload_len;

    if (buffer.size() < total_size) return "";

    uint8_t mask_key[4] = {0, 0, 0, 0};
    if (masked) {
        for (int i = 0; i < 4; i++) {
            mask_key[i] = static_cast<uint8_t>(buffer[header_size + i]);
        }
    }

    std::string payload(payload_len, '\0');
    size_t data_start = header_size + mask_size;
    for (uint64_t i = 0; i < payload_len; i++) {
        payload[i] = buffer[data_start + i] ^ mask_key[i % 4];
    }

    bytes_consumed = total_size;
    complete = true;
    return payload;
}

bool WebSocket::isUpgradeRequest(const std::string& request) {
    std::string lower = request;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower.find("upgrade: websocket") != std::string::npos;
}

std::string WebSocket::extractKey(const std::string& request) {
    std::string search = "Sec-WebSocket-Key: ";
    size_t pos = request.find(search);
    if (pos == std::string::npos) {
        search = "sec-websocket-key: ";
        pos = request.find(search);
        if (pos == std::string::npos) return "";
    }

    size_t start = pos + search.size();
    size_t end = request.find("\r\n", start);
    if (end == std::string::npos) return "";

    return request.substr(start, end - start);
}