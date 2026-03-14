#include "store.h"
#include <algorithm>
#include <random>


bool Store::checkExpired(const std::string& key) {
    auto it = expires_.find(key);
    if (it == expires_.end()) return false;

    auto now = std::chrono::steady_clock::now();
    if (now >= it->second) {
        data_.erase(key);
        expires_.erase(it);
        return true;
    }
    return false;
}

std::list<std::string>* Store::getOrCreateList(const std::string& key, bool create, bool& type_error) {
    type_error = false;
    checkExpired(key);

    auto it = data_.find(key);
    if (it == data_.end()) {
        if (!create) return nullptr;
        data_[key] = std::list<std::string>{};
        return &std::get<std::list<std::string>>(data_[key]);
    }

    if (!std::holds_alternative<std::list<std::string>>(it->second)) {
        type_error = true;
        return nullptr;
    }

    return &std::get<std::list<std::string>>(it->second);
}

SortedSet* Store::getOrCreateSortedSet(const std::string& key, bool create, bool& type_error) {
    type_error = false;
    checkExpired(key);

    auto it = data_.find(key);
    if (it == data_.end()) {
        if (!create) return nullptr;
        data_[key] = SortedSet{};
        return &std::get<SortedSet>(data_[key]);
    }

    if (!std::holds_alternative<SortedSet>(it->second)) {
        type_error = true;
        return nullptr;
    }

    return &std::get<SortedSet>(it->second);
}


void Store::set(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    expires_.erase(key);
    data_[key] = value;
}

std::optional<std::string> Store::get(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (checkExpired(key)) return std::nullopt;

    auto it = data_.find(key);
    if (it == data_.end()) return std::nullopt;

    if (!std::holds_alternative<std::string>(it->second)) {
        return std::nullopt;
    }

    return std::get<std::string>(it->second);
}


bool Store::del(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    expires_.erase(key);
    return data_.erase(key) > 0;
}

bool Store::exists(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (checkExpired(key)) return false;
    return data_.find(key) != data_.end();
}

size_t Store::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return data_.size();
}

std::string Store::type(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (checkExpired(key)) return "none";

    auto it = data_.find(key);
    if (it == data_.end()) return "none";

    if (std::holds_alternative<std::string>(it->second)) return "string";
    if (std::holds_alternative<std::list<std::string>>(it->second)) return "list";
    if (std::holds_alternative<SortedSet>(it->second)) return "zset";

    return "unknown";
}


bool Store::expire(const std::string& key, int64_t seconds) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (data_.find(key) == data_.end()) return false;

    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    expires_[key] = expiry;
    return true;
}

int64_t Store::ttl(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (data_.find(key) == data_.end()) return -2;

    auto it = expires_.find(key);
    if (it == expires_.end()) return -1;

    auto now = std::chrono::steady_clock::now();
    if (now >= it->second) {
        data_.erase(key);
        expires_.erase(it);
        return -2;
    }

    auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
        it->second - now
    );
    return remaining.count();
}

int Store::activeExpire(int max_samples) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (expires_.empty()) return 0;

    static std::mt19937 rng(std::random_device{}());

    int expired_count = 0;
    int sampled = 0;
    auto now = std::chrono::steady_clock::now();

    std::uniform_int_distribution<size_t> dist(0, expires_.size() - 1);
    size_t start_pos = dist(rng);

    auto it = expires_.begin();
    std::advance(it, start_pos);

    while (sampled < max_samples && !expires_.empty()) {
        if (it == expires_.end()) it = expires_.begin();
        if (it == expires_.end()) break;

        auto current = it;
        ++it;
        sampled++;

        if (now >= current->second) {
            data_.erase(current->first);
            expires_.erase(current);
            expired_count++;
        }
    }

    return expired_count;
}


int64_t Store::lpush(const std::string& key, const std::vector<std::string>& values) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    bool type_error = false;
    auto* lst = getOrCreateList(key, true, type_error);
    if (type_error) return -1;

    for (const auto& val : values) {
        lst->push_front(val);
    }
    return static_cast<int64_t>(lst->size());
}

int64_t Store::rpush(const std::string& key, const std::vector<std::string>& values) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    bool type_error = false;
    auto* lst = getOrCreateList(key, true, type_error);
    if (type_error) return -1;

    for (const auto& val : values) {
        lst->push_back(val);
    }
    return static_cast<int64_t>(lst->size());
}

std::optional<std::string> Store::lpop(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    bool type_error = false;
    auto* lst = getOrCreateList(key, false, type_error);
    if (type_error || lst == nullptr || lst->empty()) return std::nullopt;

    std::string value = std::move(lst->front());
    lst->pop_front();

    if (lst->empty()) {
        data_.erase(key);
    }
    return value;
}

std::optional<std::string> Store::rpop(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    bool type_error = false;
    auto* lst = getOrCreateList(key, false, type_error);
    if (type_error || lst == nullptr || lst->empty()) return std::nullopt;

    std::string value = std::move(lst->back());
    lst->pop_back();

    if (lst->empty()) {
        data_.erase(key);
    }
    return value;
}

std::optional<std::vector<std::string>> Store::lrange(const std::string& key,
                                                        int64_t start, int64_t stop) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    bool type_error = false;
    auto* lst = getOrCreateList(key, false, type_error);
    if (type_error) return std::nullopt;
    if (lst == nullptr) return std::vector<std::string>{};

    int64_t len = static_cast<int64_t>(lst->size());

    if (start < 0) start = len + start;
    if (stop < 0)  stop = len + stop;
    if (start < 0) start = 0;
    if (stop >= len) stop = len - 1;
    if (start > stop) return std::vector<std::string>{};

    std::vector<std::string> result;
    auto it = lst->begin();
    std::advance(it, start);

    for (int64_t i = start; i <= stop && it != lst->end(); i++, ++it) {
        result.push_back(*it);
    }
    return result;
}

int64_t Store::llen(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    bool type_error = false;
    auto* lst = getOrCreateList(key, false, type_error);
    if (type_error) return -1;
    if (lst == nullptr) return 0;
    return static_cast<int64_t>(lst->size());
}

int64_t Store::zadd(const std::string& key,
                     const std::vector<std::pair<double, std::string>>& entries) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    bool type_error = false;
    auto* zset = getOrCreateSortedSet(key, true, type_error);
    if (type_error) return -1;

    int64_t added = 0;
    for (const auto& [score, member] : entries) {
        if (zset->add(member, score)) added++;
    }
    return added;
}

std::optional<double> Store::zscore(const std::string& key,
                                     const std::string& member) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    bool type_error = false;
    auto* zset = getOrCreateSortedSet(key, false, type_error);
    if (type_error || zset == nullptr) return std::nullopt;

    return zset->score(member);
}

std::optional<int64_t> Store::zrank(const std::string& key,
                                     const std::string& member) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    bool type_error = false;
    auto* zset = getOrCreateSortedSet(key, false, type_error);
    if (type_error || zset == nullptr) return std::nullopt;

    return zset->rank(member);
}

std::optional<std::vector<std::string>> Store::zrange(const std::string& key,
                                                        int64_t start, int64_t stop,
                                                        bool with_scores) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    bool type_error = false;
    auto* zset = getOrCreateSortedSet(key, false, type_error);
    if (type_error) return std::nullopt;
    if (zset == nullptr) return std::vector<std::string>{};

    return zset->range(start, stop, with_scores);
}

int64_t Store::zcard(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    bool type_error = false;
    auto* zset = getOrCreateSortedSet(key, false, type_error);
    if (type_error) return -1;
    if (zset == nullptr) return 0;
    return static_cast<int64_t>(zset->size());
}

std::unordered_map<std::string, StoreValue> Store::snapshot() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return data_; // deep copy
}

std::shared_mutex& Store::getMutex() {
    return mutex_;
}