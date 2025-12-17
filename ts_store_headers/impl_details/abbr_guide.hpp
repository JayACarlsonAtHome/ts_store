// Project: ts_store
// File Path: ts_store/ts_store_headers/impl_details/abbr_guide.hpp
//
// ──────────────────────────────────────────────────────────────────────────────
// CODING STYLE & ABBREVIATION GUIDE (ts_store project)
// ──────────────────────────────────────────────────────────────────────────────
//
// | Prefix | Meaning                     | Example                         | Notes / Rule
// |--------|-----------------------------|---------------------------------|------------------------------------------------------
// | tl_    | thread_local                | tl_claimed_ids                  | Most important – always use this, never t_ or T1_
// | s_     | static (file or class)      | s_instance                      | File-static or static class member
// | g_     | true global                 | g_epoch_base                    | Very rare – avoid when possible
// | m_     | member variable (optional)  | m_rows                          | We prefer trailing underscore instead (see below)
// | _      | member variable (suffix)    | rows_, next_id_                 | Google/Abseil style – trailing underscore = member
// | k      | constexpr constant          | kDefaultPayloadSize = 80        | Usually UpperCamel after k (kMaxThreads, kBufferSize)
// | id     | identifier                  | thread_id, row_id               | Always lowercase id – never ID or Id
// | ts     | timestamp                   | ts_us, useTS_                   | Already in use throughout the code
// | us     | microseconds                | now_us(), ts_us                 | Standard time unit suffix
// | buf    | buffer                      | payload_buf                     | Obvious in context
// | pos    | position / cursor           | pos (in FastPayload)            | Clear from surrounding code
//
// ──────────────────────────────────────────────────────────────────────────────
// FORBIDDEN / CURSED PATTERNS (never use)
// ──────────────────────────────────────────────────────────────────────────────
// t_          → ambiguous (temp? thread_local?)
// T1_         → looks like a template parameter, confusing
// threadLocalIds → Java-style camelCase
// claimedIds  → when it is thread_local, loses immediate clarity
// ID / Id     → SCREAMING or PascalCase hurts readability
//
// Stick to the table above and the code stays instantly recognizable,
// self-documenting, and consistent with elite C++ codebases
// (Abseil, Folly, ClickHouse, etc.).
// ──────────────────────────────────────────────────────────────────────────────