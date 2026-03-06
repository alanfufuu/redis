#include "metrics.h"
#include <algorithm>
#include <numeric>

Metrics::Metrics()
    : active_connections_(0),
      total_requests_(0),
      total_errors_(0),
      window_start_(std::chrono::steady_clock::now()),
      window_requests_(0) {}

void Metrics::recordRequest(const std::string& command, int64_t latency_us,
                             bool success) {
    total_requests_.fetch_add(1, std::memory_order_relaxed);
    if (!success) {
        total_errors_.fetch_add(1, std::memory_order_relaxed);
    }

    RequestMetric metric;
    metric.command = command;
    metric.latency_us = latency_us;
    metric.success = success;
    metric.timestamp = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.push_back(std::move(metric));
        window_requests_++;
    }
}

void Metrics::connectionOpened() {
    active_connections_.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::connectionClosed() {
    active_connections_.fetch_sub(1, std::memory_order_relaxed);
}

int64_t Metrics::getActiveConnections() const {
    return active_connections_.load(std::memory_order_relaxed);
}

double Metrics::getCurrentRps() const {
    // This is approximate — doesn't need the lock for a rough estimate
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - window_start_
    ).count();

    if (elapsed <= 0) return 0.0;
    return (static_cast<double>(total_requests_.load()) / elapsed) * 1000.0;
}

int64_t Metrics::getTotalRequests() const {
    return total_requests_.load(std::memory_order_relaxed);
}

MetricsSnapshot Metrics::takeSnapshot(int64_t total_keys) {
    MetricsSnapshot snap;

    std::vector<RequestMetric> requests;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        requests.swap(buffer_);  // O(1) — just swaps internal pointers

        // Reset RPS window
        window_start_ = std::chrono::steady_clock::now();
        window_requests_ = 0;
    }

    snap.total_requests = total_requests_.load();
    snap.total_errors = total_errors_.load();
    snap.active_connections = active_connections_.load();
    snap.total_keys = total_keys;

    // Calculate percentiles from this batch
    if (requests.empty()) {
        snap.avg_latency_us = 0;
        snap.p50_latency_us = 0;
        snap.p95_latency_us = 0;
        snap.p99_latency_us = 0;
        snap.requests_per_second = 0;
        snap.requests = std::move(requests);
        return snap;
    }

    // Extract latencies and sort for percentile calculation
    std::vector<int64_t> latencies;
    latencies.reserve(requests.size());
    for (const auto& r : requests) {
        latencies.push_back(r.latency_us);
    }
    std::sort(latencies.begin(), latencies.end());

    // Average
    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    snap.avg_latency_us = sum / latencies.size();

    // Percentiles using nearest-rank method
    auto percentile = [&](double p) -> double {
        size_t idx = static_cast<size_t>(p / 100.0 * latencies.size());
        if (idx >= latencies.size()) idx = latencies.size() - 1;
        return static_cast<double>(latencies[idx]);
    };

    snap.p50_latency_us = percentile(50);
    snap.p95_latency_us = percentile(95);
    snap.p99_latency_us = percentile(99);

    // RPS for this batch
    if (requests.size() >= 2) {
        auto first = requests.front().timestamp;
        auto last = requests.back().timestamp;
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            last - first
        ).count();
        if (duration_ms > 0) {
            snap.requests_per_second = (static_cast<double>(requests.size()) / duration_ms) * 1000.0;
        } else {
            snap.requests_per_second = static_cast<double>(requests.size());
        }
    } else {
        snap.requests_per_second = static_cast<double>(requests.size());
    }

    snap.requests = std::move(requests);
    return snap;
}