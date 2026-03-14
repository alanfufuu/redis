#ifndef RESP_SERIALIZER_H
#define RESP_SERIALIZER_H

#include <string>
#include <vector>

class RespSerializer {
public:
    static std::string simpleString(const std::string& str);

    static std::string error(const std::string& msg);

    static std::string integer(int64_t value);

    static std::string bulkString(const std::string& str);
    static std::string nullBulkString();

    static std::string array(const std::vector<std::string>& elements);
    static std::string nullArray();

    static std::string ok();
    static std::string pong();
};

#endif