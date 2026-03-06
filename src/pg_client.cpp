#include "pg_client.h"
#include <iostream>
#include <sstream>

PgClient::PgClient(const std::string& connection_string)
    : conn_string_(connection_string), conn_(nullptr) {}

PgClient::~PgClient() {
    if (conn_) {
        PQfinish(conn_);
    }
}

bool PgClient::connect() {
    conn_ = PQconnectdb(conn_string_.c_str());

    if (PQstatus(conn_) != CONNECTION_OK) {
        std::cerr << "PostgreSQL connection failed: "
                  << PQerrorMessage(conn_) << std::endl;
        PQfinish(conn_);
        conn_ = nullptr;
        return false;
    }

    std::cout << "Connected to PostgreSQL" << std::endl;
    return true;
}

bool PgClient::isConnected() const {
    return conn_ != nullptr && PQstatus(conn_) == CONNECTION_OK;
}

std::string PgClient::escape(const std::string& str) {
    if (!conn_) return str;

    char* escaped = PQescapeLiteral(conn_, str.c_str(), str.size());
    if (!escaped) return "''";

    std::string result(escaped);
    PQfreemem(escaped);
    return result;
}

bool PgClient::insertMetrics(const std::vector<RequestMetric>& metrics) {
    if (!isConnected() || metrics.empty()) return false;

    // Build a single multi-row INSERT for efficiency.
    // Instead of 500 individual INSERTs (500 round-trips),
    // we send one INSERT with 500 value tuples (1 round-trip).
    std::ostringstream sql;
    sql << "INSERT INTO request_metrics (command, latency_us, success) VALUES ";

    for (size_t i = 0; i < metrics.size(); i++) {
        if (i > 0) sql << ",";
        sql << "("
            << escape(metrics[i].command) << ","
            << metrics[i].latency_us << ","
            << (metrics[i].success ? "true" : "false")
            << ")";
    }

    PGresult* res = PQexec(conn_, sql.str().c_str());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "PostgreSQL insert metrics failed: "
                  << PQerrorMessage(conn_) << std::endl;
        PQclear(res);
        return false;
    }

    PQclear(res);
    return true;
}

bool PgClient::insertSnapshot(const MetricsSnapshot& snapshot) {
    if (!isConnected()) return false;

    std::ostringstream sql;
    sql << "INSERT INTO server_snapshots "
        << "(total_requests, total_errors, active_connections, total_keys, "
        << "avg_latency_us, p50_latency_us, p95_latency_us, p99_latency_us, "
        << "requests_per_second) VALUES ("
        << snapshot.total_requests << ","
        << snapshot.total_errors << ","
        << snapshot.active_connections << ","
        << snapshot.total_keys << ","
        << snapshot.avg_latency_us << ","
        << snapshot.p50_latency_us << ","
        << snapshot.p95_latency_us << ","
        << snapshot.p99_latency_us << ","
        << snapshot.requests_per_second
        << ")";

    PGresult* res = PQexec(conn_, sql.str().c_str());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "PostgreSQL insert snapshot failed: "
                  << PQerrorMessage(conn_) << std::endl;
        PQclear(res);
        return false;
    }

    PQclear(res);
    return true;
}

PGconn* PgClient::getConnection() const {
    return conn_;
}


