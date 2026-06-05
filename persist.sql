//File Name: persist.sql
//Date: 2026-06-05
//Purpose - SQL Schema File
//
CREATE TABLE IF NOT EXISTS persist (
    id BIGINT PRIMARY KEY,
    thread_id BIGINT,
    per_thread_event_id BIGINT,
    flags_raw BIGINT,
    category TEXT,
    payload TEXT,
    timestamp_us BIGINT
);

