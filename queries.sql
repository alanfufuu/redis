
SELECT
    date_trunc('minute', recorded_at) AS minute,
    COUNT(*) / 60.0 AS rps
FROM request_metrics
WHERE recorded_at > NOW() - INTERVAL '1 hour'
GROUP BY minute
ORDER BY minute;


SELECT
    PERCENTILE_CONT(0.50) WITHIN GROUP (ORDER BY latency_us) AS p50,
    PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY latency_us) AS p95,
    PERCENTILE_CONT(0.99) WITHIN GROUP (ORDER BY latency_us) AS p99,
    AVG(latency_us) AS avg_latency,
    MAX(latency_us) AS max_latency
FROM request_metrics
WHERE recorded_at > NOW() - INTERVAL '1 hour';


SELECT
    command,
    COUNT(*) AS total,
    PERCENTILE_CONT(0.50) WITHIN GROUP (ORDER BY latency_us) AS p50,
    PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY latency_us) AS p95,
    AVG(latency_us) AS avg_latency
FROM request_metrics
WHERE recorded_at > NOW() - INTERVAL '1 hour'
GROUP BY command
ORDER BY total DESC;


SELECT
    date_trunc('minute', recorded_at) AS minute,
    COUNT(*) AS total,
    SUM(CASE WHEN NOT success THEN 1 ELSE 0 END) AS errors,
    ROUND(
        SUM(CASE WHEN NOT success THEN 1 ELSE 0 END)::numeric / COUNT(*) * 100,
        2
    ) AS error_rate_pct
FROM request_metrics
WHERE recorded_at > NOW() - INTERVAL '1 hour'
GROUP BY minute
ORDER BY minute;


SELECT
    recorded_at,
    requests_per_second,
    p50_latency_us,
    p95_latency_us,
    active_connections,
    total_keys
FROM server_snapshots
WHERE recorded_at > NOW() - INTERVAL '1 hour'
ORDER BY recorded_at;


SELECT
    command,
    latency_us,
    success,
    recorded_at
FROM request_metrics
WHERE recorded_at > NOW() - INTERVAL '1 hour'
ORDER BY latency_us DESC
LIMIT 20;