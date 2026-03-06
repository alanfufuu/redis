#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include "resp_parser.h"
#include "store.h"
#include <string>
#include "persistence.h"
#include "thread_pool.h"
#include "metrics.h"

class CommandHandler {
public:
    CommandHandler(Store& store);

    void setPersistence(Persistence* persistence);
    void setThreadPool(ThreadPool* pool);

    std::string execute(const RespCommand& cmd);

    void setMetrics(Metrics* metrics);

private:
    Store& store_;
    Persistence* persistence_;
    ThreadPool* thread_pool_;
    Metrics* metrics_;

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

    std::string handleSave(const RespCommand& cmd);
    std::string handleBgsave(const RespCommand& cmd);

    std::string toUpper(const std::string& str);

    std::string handleInfo(const RespCommand& cmd);


};

#endif