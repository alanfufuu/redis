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
    // Expects: $<length>\r\n<data>\r\n
    // pos should point at the '$'

    if (pos >= buffer.size()) return ParseResult::INCOMPLETE;

    if (buffer[pos] != '$') return ParseResult::ERROR;

    // Find the \r\n after the length
    size_t crlf = findCRLF(buffer, pos);
    if (crlf == std::string::npos) return ParseResult::INCOMPLETE;

    // Extract the length number between '$' and \r\n
    // e.g., "$5\r\n" → length string is "5"
    std::string length_str = buffer.substr(pos + 1, crlf - (pos + 1));

    int length;
    try {
        length = std::stoi(length_str);
    } catch (...) {
        return ParseResult::ERROR;
    }

    // Handle null bulk string ($-1\r\n)
    if (length == -1) {
        out = "";
        pos = crlf + 2;  // skip past \r\n
        return ParseResult::COMPLETE;
    }

    // Now we need `length` bytes of data, followed by \r\n
    // Data starts right after the first \r\n
    size_t data_start = crlf + 2;

    // Do we have enough bytes?  data + \r\n
    if (data_start + length + 2 > buffer.size()) {
        return ParseResult::INCOMPLETE;
    }

    // Verify the trailing \r\n after the data
    if (buffer[data_start + length] != '\r' ||
        buffer[data_start + length + 1] != '\n') {
        return ParseResult::ERROR;
    }

    // Extract the actual string data
    out = buffer.substr(data_start, length);

    // Advance pos past everything: $length\r\ndata\r\n
    pos = data_start + length + 2;

    return ParseResult::COMPLETE;
}

ParseResult RespParser::parse(const std::string& buffer, RespCommand& cmd,
                               size_t& bytes_consumed) {
    if (buffer.empty()) return ParseResult::INCOMPLETE;

    size_t pos = 0;

    // Client commands are always arrays: *<count>\r\n
    if (buffer[pos] != '*') {
        // Inline command handling (e.g., "PING\n" from netcat)
        // Tolerate both \r\n and bare \n, just like real Redis
        size_t crlf = findCRLF(buffer, pos);
        size_t lf = buffer.find('\n', pos);

        size_t line_end;
        size_t skip;

        if (crlf != std::string::npos && (lf == std::string::npos || crlf < lf)) {
            line_end = crlf;       // line content ends at \r
            skip = crlf + 2;       // consume past \r\n
        } else if (lf != std::string::npos) {
            line_end = lf;         // line content ends at \n
            skip = lf + 1;         // consume past \n
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

    // Standard RESP array parsing
    // Find the \r\n after *<count>
    size_t crlf = findCRLF(buffer, pos);
    if (crlf == std::string::npos) return ParseResult::INCOMPLETE;

    // Extract element count
    std::string count_str = buffer.substr(pos + 1, crlf - (pos + 1));
    int count;
    try {
        count = std::stoi(count_str);
    } catch (...) {
        return ParseResult::ERROR;
    }

    if (count <= 0) return ParseResult::ERROR;

    pos = crlf + 2;  // Move past *<count>\r\n

    // Parse each bulk string element
    cmd.args.clear();
    for (int i = 0; i < count; i++) {
        std::string element;
        ParseResult result = parseBulkString(buffer, pos, element);

        if (result != ParseResult::COMPLETE) {
            return result;
        }

        cmd.args.push_back(std::move(element));
    }

    // All elements parsed successfully
    bytes_consumed = pos;
    return ParseResult::COMPLETE;
}