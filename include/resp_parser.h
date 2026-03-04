#ifndef RESP_PARSER_H
#define RESP_PARSER_H

#include <string>
#include <vector>

enum class ParseResult {
    COMPLETE, 
    INCOMPLETE, 
    ERROR  
};

struct RespCommand {
    std::vector<std::string> args;
};

class RespParser {
public:
    // Try to parse one command from the buffer.
    // If successful:
    //   - fills `cmd` with the parsed command
    //   - sets `bytes_consumed` to how many bytes were used
    //   - returns COMPLETE
    // If incomplete:
    //   - returns INCOMPLETE, cmd and bytes_consumed are untouched
    // If malformed:
    //   - returns ERROR
    ParseResult parse(const std::string& buffer, RespCommand& cmd,
                      size_t& bytes_consumed);

private:
    // Parse a bulk string starting at `pos`.
    // Returns COMPLETE/INCOMPLETE/ERROR.
    // On success, advances `pos` past the bulk string.
    ParseResult parseBulkString(const std::string& buffer, size_t& pos,
                                std::string& out);

    // Find the next \r\n starting from `pos`.
    // Returns the index of \r, or std::string::npos if not found.
    size_t findCRLF(const std::string& buffer, size_t pos);
};

#endif