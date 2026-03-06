#ifndef PG_CLIENT_H
#define PG_CLIENT_H

#include "metrics.h"
#include <string>
#include <libpq-fe.h>

class PgClient {
public:
    explicit PgClient(const std::string& connection_string);
    ~PgClient();

    // Connect to PostgreSQL. Returns true on success.
    bool connect();

    // Insert a batch of request metrics
    bool insertMetrics(const std::vector<RequestMetric>& metrics);

    // Insert a server snapshot
    bool insertSnapshot(const MetricsSnapshot& snapshot);

    // Check connection status
    bool isConnected() const;

    PGconn* getConnection() const;

private:
    std::string conn_string_;
    PGconn* conn_;

    // Escape a string for safe SQL insertion
    std::string escape(const std::string& str);
};

#endif