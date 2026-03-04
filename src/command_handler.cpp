#include "command_handler.h"
#include "resp_serializer.h"
#include <algorithm>

CommandHandler::CommandHandler(Store& store) : store_(store) {}

std::string CommandHandler::toUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::string CommandHandler::execute(const RespCommand& cmd) {
    if (cmd.args.empty()) {
        return RespSerializer::error("ERR no command provided");
    }

    std::string command = toUpper(cmd.args[0]);

    if (command == "PING")    return handlePing(cmd);
    if (command == "ECHO")    return handleEcho(cmd);
    if (command == "SET")     return handleSet(cmd);
    if (command == "GET")     return handleGet(cmd);
    if (command == "DEL")     return handleDel(cmd);
    if (command == "EXISTS")  return handleExists(cmd);
    if (command == "DBSIZE")  return handleDbsize(cmd);

    return RespSerializer::error("ERR unknown command '" + cmd.args[0] + "'");
}

std::string CommandHandler::handlePing(const RespCommand& cmd) {
    // PING → +PONG\r\n
    // PING "hello" → $5\r\nhello\r\n (returns the argument as bulk string)
    if (cmd.args.size() > 1) {
        return RespSerializer::bulkString(cmd.args[1]);
    }
    return RespSerializer::pong();
}

std::string CommandHandler::handleEcho(const RespCommand& cmd) {
    // ECHO <message> → $<len>\r\n<message>\r\n
    if (cmd.args.size() < 2) {
        return RespSerializer::error("ERR wrong number of arguments for 'echo' command");
    }
    return RespSerializer::bulkString(cmd.args[1]);
}

std::string CommandHandler::handleSet(const RespCommand& cmd) {
    // SET <key> <value> → +OK\r\n
    if (cmd.args.size() < 3) {
        return RespSerializer::error("ERR wrong number of arguments for 'set' command");
    }
    store_.set(cmd.args[1], cmd.args[2]);
    return RespSerializer::ok();
}

std::string CommandHandler::handleGet(const RespCommand& cmd) {
    // GET <key> → $<len>\r\n<value>\r\n  or  $-1\r\n if not found
    if (cmd.args.size() < 2) {
        return RespSerializer::error("ERR wrong number of arguments for 'get' command");
    }
    auto value = store_.get(cmd.args[1]);
    if (value.has_value()) {
        return RespSerializer::bulkString(value.value());
    }
    return RespSerializer::nullBulkString();
}

std::string CommandHandler::handleDel(const RespCommand& cmd) {
    // DEL <key1> [key2 ...] → :<count>\r\n (number of keys deleted)
    if (cmd.args.size() < 2) {
        return RespSerializer::error("ERR wrong number of arguments for 'del' command");
    }
    int count = 0;
    for (size_t i = 1; i < cmd.args.size(); i++) {
        if (store_.del(cmd.args[i])) {
            count++;
        }
    }
    return RespSerializer::integer(count);
}

std::string CommandHandler::handleExists(const RespCommand& cmd) {
    // EXISTS <key1> [key2 ...] → :<count>\r\n
    if (cmd.args.size() < 2) {
        return RespSerializer::error("ERR wrong number of arguments for 'exists' command");
    }
    int count = 0;
    for (size_t i = 1; i < cmd.args.size(); i++) {
        if (store_.exists(cmd.args[i])) {
            count++;
        }
    }
    return RespSerializer::integer(count);
}

std::string CommandHandler::handleDbsize(const RespCommand& cmd) {
    // DBSIZE → :<count>\r\n
    (void)cmd;  // unused parameter
    return RespSerializer::integer(static_cast<int64_t>(store_.size()));
}