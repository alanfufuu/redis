#ifndef SORTED_SET_H
#define SORTED_SET_H

#include <string>
#include <set>
#include <map>
#include <unordered_map>
#include <vector>
#include <optional>

class SortedSet {
public:
    bool add(const std::string& member, double score);

    std::optional<double> score(const std::string& member) const;

    std::optional<int64_t> rank(const std::string& member) const;

    std::vector<std::string> range(int64_t start, int64_t stop, bool with_scores = false) const;

    bool remove(const std::string& member);

    size_t size() const;
    bool empty() const;

private:
    std::map<double, std::set<std::string>> score_to_members_;
    std::unordered_map<std::string, double> member_to_score_;
};

#endif