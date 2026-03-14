#ifndef METRICS_H
#define METRICS_H

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <atomic>
#include <cstdint>

struct RequestMetric {
    std::string command;
    int64_t latency_us;   // microseconds
    bool success;
    std::chrono::steady_clock::time_point timestamp;
};

struct MetricsSnapshot {
    // Per-request data (for PostgreSQL batching)
    std::vector<RequestMetric> requests;

    // Aggregate stats (for dashboard)
    int64_t total_requests;
    int64_t total_errors;
    double avg_latency_us;
    double p50_latency_us;
    double p95_latency_us;
    double p99_latency_us;
    double requests_per_second;
    int64_t active_connections;
    int64_t total_keys;
};

class Metrics {
public:
    Metrics();

    void recordRequest(const std::string& command, int64_t latency_us, bool success);

    void connectionOpened();
    void connectionClosed();
    int64_t getActiveConnections() const;

    MetricsSnapshot takeSnapshot(int64_t total_keys);

    double getCurrentRps() const;

    int64_t getTotalRequests() const;

private:
    mutable std::mutex mutex_;
    std::vector<RequestMetric> buffer_;

    std::atomic<int64_t> active_connections_;
    std::atomic<int64_t> total_requests_;
    std::atomic<int64_t> total_errors_;

    std::chrono::steady_clock::time_point window_start_;
    int64_t window_requests_;
};

#endif