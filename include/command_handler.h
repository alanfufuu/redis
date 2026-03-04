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

    std::string handlePing(const RespCommand& cmd);
    std::string handleEcho(const RespCommand& cmd);
    std::string handleSet(const RespCommand& cmd);
    std::string handleGet(const RespCommand& cmd);
    std::string handleDel(const RespCommand& cmd);
    std::string handleExists(const RespCommand& cmd);
    std::string handleDbsize(const RespCommand& cmd);

    std::string toUpper(const std::string& str);
};

#endif