// Project: ts_store
// File Path: ts_store/ts_store_headers/impl_details/abbr_guide.hpp
//
// ──────────────────────────────────────────────────────────────────────────────
// CODING STYLE & ABBREVIATION GUIDE (ts_store project)
// ──────────────────────────────────────────────────────────────────────────────
//
// | Prefix/Suffix | Meaning                     | Example                         | Notes / Rule
// |---------------|-----------------------------|---------------------------------|------------------------------------------------------
// | s_            | static (file or class)      | s_epoch_base                    | File-static or static class member
// | _             | member variable (suffix)    | rows_, next_id_                 | Google/Abseil style – trailing underscore = member (preferred)
// | k             | constexpr constant          | kMaxThreads, kBufferSize        | UpperCamel after k
// | id            | identifier                  | thread_id, event_id, row_id     | Always lowercase id – never ID or Id
// | ts            | timestamp                   | ts_us                           | Time-related fields
// | us            | microseconds                | ts_us, now_us()                 | Standard time unit suffix
//
// ──────────────────────────────────────────────────────────────────────────────
// FORBIDDEN / CURSED PATTERNS (never use)
// ──────────────────────────────────────────────────────────────────────────────
// t_            → ambiguous (temp? thread_local?)
// T1_           → looks like a template parameter, confusing
// threadLocalIds → Java-style camelCase
// claimedIds    → when it is thread_local/static, loses immediate clarity
// ID / Id       → SCREAMING or PascalCase hurts readability
// m_            → prefix for members (use trailing _ instead)
//
// Stick to the table above and the code stays instantly recognizable,
// self-documenting, and consistent with elite C++ codebases
// (Abseil, Folly, ClickHouse, etc.).
// ──────────────────────────────────────────────────────────────────────────────