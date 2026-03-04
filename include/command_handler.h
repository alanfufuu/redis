#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include "resp_parser.h"
#include "store.h"
#include <string>

class CommandHandler {
public:
    CommandHandler(Store& store);
    std::string execute(const RespCommand& cmd);

private:
    Store& store_;

    // string
    std::string handlePing(const RespCommand& cmd);
    std::string handleEcho(const RespCommand& cmd);
    std::string handleSet(const RespCommand& cmd);
    std::string handleGet(const RespCommand& cmd);
    std::string handleDel(const RespCommand& cmd);
    std::string handleExists(const RespCommand& cmd);
    std::string handleDbsize(const RespCommand& cmd);
    std::string handleType(const RespCommand& cmd);

    // list
    std::string handleLpush(const RespCommand& cmd);
    std::string handleRpush(const RespCommand& cmd);
    std::string handleLpop(const RespCommand& cmd);
    std::string handleRpop(const RespCommand& cmd);
    std::string handleLrange(const RespCommand& cmd);
    std::string handleLlen(const RespCommand& cmd);

    // Sorted set commands
    std::string handleZadd(const RespCommand& cmd);
    std::string handleZscore(const RespCommand& cmd);
    std::string handleZrank(const RespCommand& cmd);
    std::string handleZrange(const RespCommand& cmd);
    std::string handleZcard(const RespCommand& cmd);

    // TTL commands
    std::string handleExpire(const RespCommand& cmd);
    std::string handleTtl(const RespCommand& cmd);

    std::string toUpper(const std::string& str);
};

#endif