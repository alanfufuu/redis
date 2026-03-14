#include "sorted_set.h"
#include <sstream>
#include <iomanip>

bool SortedSet::add(const std::string& member, double score) {
    auto it = member_to_score_.find(member);

    if (it != member_to_score_.end()) {
        double old_score = it->second;

        if (old_score == score) return false;  

        auto& old_bucket = score_to_members_[old_score];
        old_bucket.erase(member);
        if (old_bucket.empty()) {
            score_to_members_.erase(old_score);
        }
        score_to_members_[score].insert(member);
        it->second = score;

        return false; 
    }

    member_to_score_[member] = score;
    score_to_members_[score].insert(member);

    return true;
}

std::optional<double> SortedSet::score(const std::string& member) const {
    auto it = member_to_score_.find(member);
    if (it == member_to_score_.end()) return std::nullopt;
    return it->second;
}

std::optional<int64_t> SortedSet::rank(const std::string& member) const {
    auto score_it = member_to_score_.find(member);
    if (score_it == member_to_score_.end()) return std::nullopt;

    double target_score = score_it->second;
    int64_t rank = 0;

    for (const auto& [score, members] : score_to_members_) {
        if (score < target_score) {
            rank += static_cast<int64_t>(members.size());
        } else if (score == target_score) {
            
            for (const auto& m : members) {
                if (m == member) break;
                rank++;
            }
            break;
        } else {
            break; 
        }
    }

    return rank;
}

std::vector<std::string> SortedSet::range(int64_t start, int64_t stop,
                                           bool with_scores) const {
    int64_t len = static_cast<int64_t>(member_to_score_.size());

    if (start < 0) start = len + start;
    if (stop < 0)  stop = len + stop;

    if (start < 0) start = 0;
    if (stop >= len) stop = len - 1;

    if (start > stop) return {};

    std::vector<std::string> result;
    int64_t current_rank = 0;

    for (const auto& [score, members] : score_to_members_) {
        for (const auto& member : members) {
            if (current_rank >= start && current_rank <= stop) {
                result.push_back(member);
                if (with_scores) {
                    std::ostringstream oss;
                    oss << score;
                    result.push_back(oss.str());
                }
            }
            current_rank++;
            if (current_rank > stop) break;
        }
        if (current_rank > stop) break;
    }

    return result;
}

bool SortedSet::remove(const std::string& member) {
    auto it = member_to_score_.find(member);
    if (it == member_to_score_.end()) return false;

    double score = it->second;

    auto& bucket = score_to_members_[score];
    bucket.erase(member);
    if (bucket.empty()) {
        score_to_members_.erase(score);
    }

    member_to_score_.erase(it);

    return true;
}

size_t SortedSet::size() const {
    return member_to_score_.size();
}

bool SortedSet::empty() const {
    return member_to_score_.empty();
}