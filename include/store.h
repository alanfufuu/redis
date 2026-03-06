#ifndef STORE_H
#define STORE_H

#include <string>
#include <list>
#include <unordered_map>
#include <optional>
#include <variant>
#include <vector>
#include "sorted_set.h"
#include <chrono>
#include <shared_mutex>

using StoreValue = std::variant<std::string, std::list<std::string>, SortedSet>;
using TimePoint = std::chrono::steady_clock::time_point;

class Store {
public:
    // String operations
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);

    // Key operations
    bool del(const std::string& key);
    bool exists(const std::string& key);
    size_t size() const;
    std::string type(const std::string& key);

    // TTL operations
    bool expire(const std::string& key, int64_t seconds);
    int64_t ttl(const std::string& key);
    int activeExpire(int max_samples = 20);

    // List operations
    int64_t lpush(const std::string& key, const std::vector<std::string>& values);
    int64_t rpush(const std::string& key, const std::vector<std::string>& values);
    std::optional<std::string> lpop(const std::string& key);
    std::optional<std::string> rpop(const std::string& key);
    std::optional<std::vector<std::string>> lrange(const std::string& key, int64_t start, int64_t stop);
    int64_t llen(const std::string& key);

    // Sorted set operations
    int64_t zadd(const std::string& key, const std::vector<std::pair<double, std::string>>& entries);
    std::optional<double> zscore(const std::string& key, const std::string& member);
    std::optional<int64_t> zrank(const std::string& key, const std::string& member);
    std::optional<std::vector<std::string>> zrange(const std::string& key, int64_t start, int64_t stop, bool with_scores = false);
    int64_t zcard(const std::string& key);

    // snapshot
    std::unordered_map<std::string, StoreValue> snapshot() const;

    std::shared_mutex& getMutex();

private:
    std::unordered_map<std::string, StoreValue> data_;
    std::unordered_map<std::string, TimePoint> expires_;
    mutable std::shared_mutex mutex_;

    std::list<std::string>* getOrCreateList(const std::string& key, bool create, bool& type_error);
    SortedSet* getOrCreateSortedSet(const std::string& key, bool create, bool& type_error);

    bool checkExpired(const std::string& key);
};

#endif