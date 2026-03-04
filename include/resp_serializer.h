#ifndef RESP_SERIALIZER_H
#define RESP_SERIALIZER_H

#include <string>
#include <vector>

class RespSerializer {
public:
    // +OK\r\n
    static std::string simpleString(const std::string& str);

    // -ERR message\r\n
    static std::string error(const std::string& msg);

    // :<number>\r\n
    static std::string integer(int64_t value);

    // $<length>\r\n<data>\r\n   or   $-1\r\n for null
    static std::string bulkString(const std::string& str);
    static std::string nullBulkString();

    // *<count>\r\n followed by each element
    static std::string array(const std::vector<std::string>& elements);
    static std::string nullArray();

    // Convenience shortcuts
    static std::string ok();
    static std::string pong();
};

#endif