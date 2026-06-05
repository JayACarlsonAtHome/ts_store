//File:    test002.sql
//Date:    2026-06-05
//Purpose: SQL Schema File
//Related: type=ts_store table=test002
//
CREATE TABLE IF NOT EXISTS test002 (
    id BIGINT PRIMARY KEY,
    thread_id BIGINT,
    per_thread_event_id BIGINT,
    flags_raw BIGINT,
    category TEXT,
    payload TEXT,
    timestamp_us BIGINT
);

