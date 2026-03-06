#include "command_handler.h"
#include "resp_serializer.h"
#include <algorithm>
#include <sstream>
#include "persistence.h"

CommandHandler::CommandHandler(Store& store)
    : store_(store), persistence_(nullptr), thread_pool_(nullptr), metrics_(nullptr) {}

void CommandHandler::setPersistence(Persistence* persistence) {
    persistence_ = persistence;
}

void CommandHandler::setThreadPool(ThreadPool* pool) {
    thread_pool_ = pool;
}

void CommandHandler::setMetrics(Metrics* metrics) {
    metrics_ = metrics;
}

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
    if (command == "TYPE")    return handleType(cmd);
    if (command == "LPUSH")   return handleLpush(cmd);
    if (command == "RPUSH")   return handleRpush(cmd);
    if (command == "LPOP")    return handleLpop(cmd);
    if (command == "RPOP")    return handleRpop(cmd);
    if (command == "LRANGE")  return handleLrange(cmd);
    if (command == "LLEN")    return handleLlen(cmd);
    if (command == "ZADD")    return handleZadd(cmd);
    if (command == "ZSCORE")  return handleZscore(cmd);
    if (command == "ZRANK")   return handleZrank(cmd);
    if (command == "ZRANGE")  return handleZrange(cmd);
    if (command == "ZCARD")   return handleZcard(cmd);
    if (command == "EXPIRE")  return handleExpire(cmd);
    if (command == "TTL")     return handleTtl(cmd);
    if (command == "SAVE")    return handleSave(cmd);
    if (command == "BGSAVE")  return handleBgsave(cmd);
    if (command == "INFO")    return handleInfo(cmd);

    return RespSerializer::error("ERR unknown command '" + cmd.args[0] + "'");
}

// ==================== Existing handlers (unchanged) ====================

std::string CommandHandler::handlePing(const RespCommand& cmd) {
    if (cmd.args.size() > 1) {
        return RespSerializer::bulkString(cmd.args[1]);
    }
    return RespSerializer::pong();
}

std::string CommandHandler::handleEcho(const RespCommand& cmd) {
    if (cmd.args.size() < 2) {
        return RespSerializer::error("ERR wrong number of arguments for 'echo' command");
    }
    return RespSerializer::bulkString(cmd.args[1]);
}

std::string CommandHandler::handleSet(const RespCommand& cmd) {
    if (cmd.args.size() < 3) {
        return RespSerializer::error("ERR wrong number of arguments for 'set' command");
    }
    store_.set(cmd.args[1], cmd.args[2]);
    return RespSerializer::ok();
}

std::string CommandHandler::handleGet(const RespCommand& cmd) {
    if (cmd.args.size() < 2) {
        return RespSerializer::error("ERR wrong number of arguments for 'get' command");
    }

    // Check if key exists but is wrong type
    if (store_.exists(cmd.args[1]) && store_.type(cmd.args[1]) != "string") {
        return RespSerializer::error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    auto value = store_.get(cmd.args[1]);
    if (value.has_value()) {
        return RespSerializer::bulkString(value.value());
    }
    return RespSerializer::nullBulkString();
}

std::string CommandHandler::handleDel(const RespCommand& cmd) {
    if (cmd.args.size() < 2) {
        return RespSerializer::error("ERR wrong number of arguments for 'del' command");
    }
    int count = 0;
    for (size_t i = 1; i < cmd.args.size(); i++) {
        if (store_.del(cmd.args[i])) count++;
    }
    return RespSerializer::integer(count);
}

std::string CommandHandler::handleExists(const RespCommand& cmd) {
    if (cmd.args.size() < 2) {
        return RespSerializer::error("ERR wrong number of arguments for 'exists' command");
    }
    int count = 0;
    for (size_t i = 1; i < cmd.args.size(); i++) {
        if (store_.exists(cmd.args[i])) count++;
    }
    return RespSerializer::integer(count);
}

std::string CommandHandler::handleDbsize(const RespCommand& cmd) {
    (void)cmd;
    return RespSerializer::integer(static_cast<int64_t>(store_.size()));
}

std::string CommandHandler::handleType(const RespCommand& cmd) {
    if (cmd.args.size() < 2) {
        return RespSerializer::error("ERR wrong number of arguments for 'type' command");
    }
    return RespSerializer::simpleString(store_.type(cmd.args[1]));
}

// ==================== List handlers ====================

std::string CommandHandler::handleLpush(const RespCommand& cmd) {
    if (cmd.args.size() < 3) {
        return RespSerializer::error("ERR wrong number of arguments for 'lpush' command");
    }
    std::vector<std::string> values(cmd.args.begin() + 2, cmd.args.end());
    int64_t new_len = store_.lpush(cmd.args[1], values);
    if (new_len == -1) {
        return RespSerializer::error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    return RespSerializer::integer(new_len);
}

std::string CommandHandler::handleRpush(const RespCommand& cmd) {
    if (cmd.args.size() < 3) {
        return RespSerializer::error("ERR wrong number of arguments for 'rpush' command");
    }
    std::vector<std::string> values(cmd.args.begin() + 2, cmd.args.end());
    int64_t new_len = store_.rpush(cmd.args[1], values);
    if (new_len == -1) {
        return RespSerializer::error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    return RespSerializer::integer(new_len);
}

std::string CommandHandler::handleLpop(const RespCommand& cmd) {
    if (cmd.args.size() < 2) {
        return RespSerializer::error("ERR wrong number of arguments for 'lpop' command");
    }

    // Check for WRONGTYPE
    if (store_.exists(cmd.args[1]) && store_.type(cmd.args[1]) != "list") {
        return RespSerializer::error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    auto value = store_.lpop(cmd.args[1]);
    if (value.has_value()) {
        return RespSerializer::bulkString(value.value());
    }
    return RespSerializer::nullBulkString();
}

std::string CommandHandler::handleRpop(const RespCommand& cmd) {
    if (cmd.args.size() < 2) {
        return RespSerializer::error("ERR wrong number of arguments for 'rpop' command");
    }

    if (store_.exists(cmd.args[1]) && store_.type(cmd.args[1]) != "list") {
        return RespSerializer::error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    auto value = store_.rpop(cmd.args[1]);
    if (value.has_value()) {
        return RespSerializer::bulkString(value.value());
    }
    return RespSerializer::nullBulkString();
}

std::string CommandHandler::handleLrange(const RespCommand& cmd) {
    if (cmd.args.size() < 4) {
        return RespSerializer::error("ERR wrong number of arguments for 'lrange' command");
    }

    int64_t start, stop;
    try {
        start = std::stoll(cmd.args[2]);
        stop = std::stoll(cmd.args[3]);
    } catch (...) {
        return RespSerializer::error("ERR value is not an integer or out of range");
    }

    // Check for WRONGTYPE
    if (store_.exists(cmd.args[1]) && store_.type(cmd.args[1]) != "list") {
        return RespSerializer::error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    auto result = store_.lrange(cmd.args[1], start, stop);
    if (!result.has_value()) {
        return RespSerializer::error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    // Build an array of bulk strings
    std::vector<std::string> elements;
    for (const auto& val : result.value()) {
        elements.push_back(RespSerializer::bulkString(val));
    }
    return RespSerializer::array(elements);
}

std::string CommandHandler::handleLlen(const RespCommand& cmd) {
    if (cmd.args.size() < 2) {
        return RespSerializer::error("ERR wrong number of arguments for 'llen' command");
    }

    if (store_.exists(cmd.args[1]) && store_.type(cmd.args[1]) != "list") {
        return RespSerializer::error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    return RespSerializer::integer(store_.llen(cmd.args[1]));
}


std::string CommandHandler::handleZadd(const RespCommand& cmd) {
    // ZADD key score1 member1 [score2 member2 ...]
    if (cmd.args.size() < 4 || (cmd.args.size() - 2) % 2 != 0) {
        return RespSerializer::error("ERR wrong number of arguments for 'zadd' command");
    }

    std::vector<std::pair<double, std::string>> entries;
    for (size_t i = 2; i < cmd.args.size(); i += 2) {
        double score;
        try {
            score = std::stod(cmd.args[i]);
        } catch (...) {
            return RespSerializer::error("ERR value is not a valid float");
        }
        entries.emplace_back(score, cmd.args[i + 1]);
    }

    int64_t added = store_.zadd(cmd.args[1], entries);
    if (added == -1) {
        return RespSerializer::error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    return RespSerializer::integer(added);
}

std::string CommandHandler::handleZscore(const RespCommand& cmd) {
    // ZSCORE key member
    if (cmd.args.size() < 3) {
        return RespSerializer::error("ERR wrong number of arguments for 'zscore' command");
    }

    if (store_.exists(cmd.args[1]) && store_.type(cmd.args[1]) != "zset") {
        return RespSerializer::error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    auto score = store_.zscore(cmd.args[1], cmd.args[2]);
    if (!score.has_value()) {
        return RespSerializer::nullBulkString();
    }

    std::ostringstream oss;
    oss << score.value();
    return RespSerializer::bulkString(oss.str());
}

std::string CommandHandler::handleZrank(const RespCommand& cmd) {
    // ZRANK key member
    if (cmd.args.size() < 3) {
        return RespSerializer::error("ERR wrong number of arguments for 'zrank' command");
    }

    if (store_.exists(cmd.args[1]) && store_.type(cmd.args[1]) != "zset") {
        return RespSerializer::error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    auto rank = store_.zrank(cmd.args[1], cmd.args[2]);
    if (!rank.has_value()) {
        return RespSerializer::nullBulkString();
    }
    return RespSerializer::integer(rank.value());
}

std::string CommandHandler::handleZrange(const RespCommand& cmd) {
    // ZRANGE key start stop [WITHSCORES]
    if (cmd.args.size() < 4) {
        return RespSerializer::error("ERR wrong number of arguments for 'zrange' command");
    }

    int64_t start, stop;
    try {
        start = std::stoll(cmd.args[2]);
        stop = std::stoll(cmd.args[3]);
    } catch (...) {
        return RespSerializer::error("ERR value is not an integer or out of range");
    }

    bool with_scores = false;
    if (cmd.args.size() >= 5 && toUpper(cmd.args[4]) == "WITHSCORES") {
        with_scores = true;
    }

    if (store_.exists(cmd.args[1]) && store_.type(cmd.args[1]) != "zset") {
        return RespSerializer::error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    auto result = store_.zrange(cmd.args[1], start, stop, with_scores);
    if (!result.has_value()) {
        return RespSerializer::error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    std::vector<std::string> elements;
    for (const auto& val : result.value()) {
        elements.push_back(RespSerializer::bulkString(val));
    }
    return RespSerializer::array(elements);
}

std::string CommandHandler::handleZcard(const RespCommand& cmd) {
    // ZCARD key
    if (cmd.args.size() < 2) {
        return RespSerializer::error("ERR wrong number of arguments for 'zcard' command");
    }

    if (store_.exists(cmd.args[1]) && store_.type(cmd.args[1]) != "zset") {
        return RespSerializer::error("WRONGTYPE Operation against a key holding the wrong kind of value");
    }

    return RespSerializer::integer(store_.zcard(cmd.args[1]));
}

std::string CommandHandler::handleExpire(const RespCommand& cmd) {
    // EXPIRE key seconds → :1 if set, :0 if key doesn't exist
    if (cmd.args.size() < 3) {
        return RespSerializer::error("ERR wrong number of arguments for 'expire' command");
    }

    int64_t seconds;
    try {
        seconds = std::stoll(cmd.args[2]);
    } catch (...) {
        return RespSerializer::error("ERR value is not an integer or out of range");
    }

    bool result = store_.expire(cmd.args[1], seconds);
    return RespSerializer::integer(result ? 1 : 0);
}

std::string CommandHandler::handleTtl(const RespCommand& cmd) {
    // TTL key → seconds remaining, -1 if no TTL, -2 if key doesn't exist
    if (cmd.args.size() < 2) {
        return RespSerializer::error("ERR wrong number of arguments for 'ttl' command");
    }

    return RespSerializer::integer(store_.ttl(cmd.args[1]));
}


std::string CommandHandler::handleSave(const RespCommand& cmd) {
    (void)cmd;
    if (!persistence_) {
        return RespSerializer::error("ERR persistence not configured");
    }

    // Synchronous save — blocks the event loop
    auto data = store_.snapshot();
    bool success = persistence_->save(data);

    if (success) {
        return RespSerializer::ok();
    }
    return RespSerializer::error("ERR snapshot failed");
}

std::string CommandHandler::handleBgsave(const RespCommand& cmd) {
    (void)cmd;
    if (!persistence_ || !thread_pool_) {
        return RespSerializer::error("ERR persistence or thread pool not configured");
    }

    auto data = std::make_shared<std::unordered_map<std::string, StoreValue>>(
        store_.snapshot()
    );

    Persistence* pers = persistence_;
    thread_pool_->submit([pers, data]() {
        pers->save(*data);
    });

    return RespSerializer::simpleString("Background saving started");
}

std::string CommandHandler::handleInfo(const RespCommand& cmd) {
    (void)cmd;
    if (!metrics_) {
        return RespSerializer::error("ERR metrics not configured");
    }

    auto snap = metrics_->takeSnapshot(static_cast<int64_t>(store_.size()));

    std::string info;
    info += "# Server\r\n";
    info += "total_requests:" + std::to_string(snap.total_requests) + "\r\n";
    info += "total_errors:" + std::to_string(snap.total_errors) + "\r\n";
    info += "active_connections:" + std::to_string(snap.active_connections) + "\r\n";
    info += "total_keys:" + std::to_string(snap.total_keys) + "\r\n";
    info += "\r\n";
    info += "# Latency (microseconds)\r\n";
    info += "avg_latency_us:" + std::to_string(snap.avg_latency_us) + "\r\n";
    info += "p50_latency_us:" + std::to_string(snap.p50_latency_us) + "\r\n";
    info += "p95_latency_us:" + std::to_string(snap.p95_latency_us) + "\r\n";
    info += "p99_latency_us:" + std::to_string(snap.p99_latency_us) + "\r\n";
    info += "\r\n";
    info += "# Throughput\r\n";
    info += "requests_per_second:" + std::to_string(snap.requests_per_second) + "\r\n";

    return RespSerializer::bulkString(info);
}
