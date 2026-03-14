#include "resp_parser.h"
#include <iostream>

size_t RespParser::findCRLF(const std::string& buffer, size_t pos) {

    while (pos + 1 < buffer.size()) {
        size_t found = buffer.find('\r', pos);
        if (found == std::string::npos || found + 1 >= buffer.size()) {
            return std::string::npos;
        }
        if (buffer[found + 1] == '\n') {
            return found;
        }
        pos = found + 1;
    }
    return std::string::npos;
}

ParseResult RespParser::parseBulkString(const std::string& buffer,
                                         size_t& pos, std::string& out) {


    if (pos >= buffer.size()) return ParseResult::INCOMPLETE;

    if (buffer[pos] != '$') return ParseResult::ERROR;


    size_t crlf = findCRLF(buffer, pos);
    if (crlf == std::string::npos) return ParseResult::INCOMPLETE;

    std::string length_str = buffer.substr(pos + 1, crlf - (pos + 1));

    int length;
    try {
        length = std::stoi(length_str);
    } catch (...) {
        return ParseResult::ERROR;
    }

    if (length == -1) {
        out = "";
        pos = crlf + 2;  
        return ParseResult::COMPLETE;
    }

    size_t data_start = crlf + 2;

    if (data_start + length + 2 > buffer.size()) {
        return ParseResult::INCOMPLETE;
    }

    if (buffer[data_start + length] != '\r' ||
        buffer[data_start + length + 1] != '\n') {
        return ParseResult::ERROR;
    }

    out = buffer.substr(data_start, length);

    pos = data_start + length + 2;

    return ParseResult::COMPLETE;
}

ParseResult RespParser::parse(const std::string& buffer, RespCommand& cmd,
                               size_t& bytes_consumed) {
    if (buffer.empty()) return ParseResult::INCOMPLETE;

    size_t pos = 0;

    if (buffer[pos] != '*') {
        size_t crlf = findCRLF(buffer, pos);
        size_t lf = buffer.find('\n', pos);

        size_t line_end;
        size_t skip;

        if (crlf != std::string::npos && (lf == std::string::npos || crlf < lf)) {
            line_end = crlf;   
            skip = crlf + 2;  
        } else if (lf != std::string::npos) {
            line_end = lf; 
            skip = lf + 1;  
        } else {
            return ParseResult::INCOMPLETE;
        }

        std::string line = buffer.substr(0, line_end);
        cmd.args.clear();

        size_t start = 0;
        while (start < line.size()) {
            while (start < line.size() && line[start] == ' ') start++;
            if (start >= line.size()) break;

            size_t end = line.find(' ', start);
            if (end == std::string::npos) end = line.size();

            cmd.args.push_back(line.substr(start, end - start));
            start = end;
        }

        if (cmd.args.empty()) return ParseResult::ERROR;

        bytes_consumed = skip;
        return ParseResult::COMPLETE;
    }

    size_t crlf = findCRLF(buffer, pos);
    if (crlf == std::string::npos) return ParseResult::INCOMPLETE;

    std::string count_str = buffer.substr(pos + 1, crlf - (pos + 1));
    int count;
    try {
        count = std::stoi(count_str);
    } catch (...) {
        return ParseResult::ERROR;
    }

    if (count <= 0) return ParseResult::ERROR;

    pos = crlf + 2;

    cmd.args.clear();
    for (int i = 0; i < count; i++) {
        std::string element;
        ParseResult result = parseBulkString(buffer, pos, element);

        if (result != ParseResult::COMPLETE) {
            return result;
        }

        cmd.args.push_back(std::move(element));
    }

    bytes_consumed = pos;
    return ParseResult::COMPLETE;
}