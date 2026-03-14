#ifndef PG_CLIENT_H
#define PG_CLIENT_H

#include "metrics.h"
#include <string>
#include <libpq-fe.h>

class PgClient {
public:
    explicit PgClient(const std::string& connection_string);
    ~PgClient();

    bool connect();

    bool insertMetrics(const std::vector<RequestMetric>& metrics);

    bool insertSnapshot(const MetricsSnapshot& snapshot);

    bool isConnected() const;

    PGconn* getConnection() const;

private:
    std::string conn_string_;
    PGconn* conn_;

    std::string escape(const std::string& str);
};

#endif