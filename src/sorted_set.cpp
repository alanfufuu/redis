#include "sorted_set.h"
#include <sstream>
#include <iomanip>

bool SortedSet::add(const std::string& member, double score) {
    auto it = member_to_score_.find(member);

    if (it != member_to_score_.end()) {
        // Member already exists — update its score
        double old_score = it->second;

        if (old_score == score) return false;  // No change needed

        // Remove from old score bucket
        auto& old_bucket = score_to_members_[old_score];
        old_bucket.erase(member);
        if (old_bucket.empty()) {
            score_to_members_.erase(old_score);
        }

        // Insert into new score bucket
        score_to_members_[score].insert(member);
        it->second = score;

        return false;  // Not a new member, just updated
    }

    // Brand new member
    member_to_score_[member] = score;
    score_to_members_[score].insert(member);

    return true;  // New member added
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

    // Walk through the score map in order.
    // Every bucket before the target score contributes its full size.
    // In the target bucket, count members alphabetically before this one.
    for (const auto& [score, members] : score_to_members_) {
        if (score < target_score) {
            rank += static_cast<int64_t>(members.size());
        } else if (score == target_score) {
            // Count members in this bucket that come before our member
            for (const auto& m : members) {
                if (m == member) break;
                rank++;
            }
            break;
        } else {
            break;  // Past our score, stop
        }
    }

    return rank;
}

std::vector<std::string> SortedSet::range(int64_t start, int64_t stop,
                                           bool with_scores) const {
    int64_t len = static_cast<int64_t>(member_to_score_.size());

    // Handle negative indices
    if (start < 0) start = len + start;
    if (stop < 0)  stop = len + stop;

    // Clamp
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
                    // Format score without unnecessary trailing zeros
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

    // Remove from score bucket
    auto& bucket = score_to_members_[score];
    bucket.erase(member);
    if (bucket.empty()) {
        score_to_members_.erase(score);
    }

    // Remove from member map
    member_to_score_.erase(it);

    return true;
}

size_t SortedSet::size() const {
    return member_to_score_.size();
}

bool SortedSet::empty() const {
    return member_to_score_.empty();
}