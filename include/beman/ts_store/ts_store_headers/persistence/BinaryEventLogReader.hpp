#pragma once

// BinaryEventLogReader.hpp
// Reader for the fast binary format produced by BinaryEventLog.
// Also provides conversion helpers to jText for inspection.

#include <string>
#include <string_view>
#include <vector>
#include <fstream>
#include <cstdint>
#include <optional>
#include <functional>

#ifdef TS_STORE_ENABLE_JTEXT_PERSIST
#include "jText.h"
#endif

namespace jac::ts_store::inline_v001 {

struct BinaryRecord {
    uint64_t event_id = 0;
    uint64_t thread_id = 0;
    uint64_t per_thread_event_id = 0;
    uint64_t raw_flags = 0;
    uint64_t timestamp_us = 0;
    std::string category;
    std::string payload;
    std::vector<int64_t> int_metrics;
    std::vector<double> dbl_metrics;
};

class BinaryEventLogReader {
public:
    explicit BinaryEventLogReader(std::string_view filepath);

    // Returns true if a record was read
    bool next(BinaryRecord& out_record);

    // Rewind to beginning
    void rewind();

    // Total records seen so far (approximate until end)
    [[nodiscard]] size_t records_read() const { return records_read_; }

    // Convert the entire binary log to jText files (for debugging/inspection)
    // This creates three jText files with the same naming convention as JTextSplitEventLog
#ifdef TS_STORE_ENABLE_JTEXT_PERSIST
    void convert_to_jtext(std::string_view output_base_name,
                          size_t int_count,
                          size_t dbl_count) const;
#endif

private:
    bool read_next_record(BinaryRecord& out);

    std::ifstream file_;
    std::string filepath_;
    size_t records_read_ = 0;
    bool eof_reached_ = false;
};

} // namespace jac::ts_store::inline_v001
