#include "resp_serializer.h"

std::string RespSerializer::simpleString(const std::string& str) {
    // +<string>\r\n
    return "+" + str + "\r\n";
}

std::string RespSerializer::error(const std::string& msg) {
    // -<error message>\r\n
    return "-" + msg + "\r\n";
}

std::string RespSerializer::integer(int64_t value) {
    // :<number>\r\n
    return ":" + std::to_string(value) + "\r\n";
}

std::string RespSerializer::bulkString(const std::string& str) {
    // $<length>\r\n<data>\r\n
    return "$" + std::to_string(str.size()) + "\r\n" + str + "\r\n";
}

std::string RespSerializer::nullBulkString() {
    // $-1\r\n  — represents a null/missing value
    return "$-1\r\n";
}

std::string RespSerializer::array(const std::vector<std::string>& elements) {
    // *<count>\r\n followed by each element (already serialized)
    std::string result = "*" + std::to_string(elements.size()) + "\r\n";
    for (const auto& elem : elements) {
        result += elem;
    }
    return result;
}

std::string RespSerializer::nullArray() {
    // *-1\r\n
    return "*-1\r\n";
}

std::string RespSerializer::ok() {
    return simpleString("OK");
}

std::string RespSerializer::pong() {
    return simpleString("PONG");
}