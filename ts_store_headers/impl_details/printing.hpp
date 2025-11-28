
// Project: ts_store
// File Path: ts_store/ts_store_headers/impl_details/printing.hpp
//

void print(std::ostream& os = std::cout, int sort_mode = 0, size_t max_rows = 10'000) const
{
    std::shared_lock lock(data_mtx_);

    // Collect all IDs directly from the map – guaranteed complete and correct
    std::vector<std::uint64_t> ids;
    ids.reserve(rows_.size());
    for (const auto& pair : rows_) {
        ids.push_back(pair.first);
    }

    // Sort by ID → chronological order (because next_id_ is monotonic)
    if (sort_mode == 0) {
        std::sort(ids.begin(), ids.end());
    }
    // Optional: future sort_mode 1/2 can be added cleanly later

    if (max_rows > 0 && ids.size() > max_rows) {
        os << "[Truncating output: showing first " << max_rows
           << " of " << ids.size() << " entries]\n\n";
        ids.resize(max_rows);
    }

    if (ids.empty()) {
        os << "ts_store<" << Threads << "," << WorkersPerThread
           << "," << BufferSize << "> is empty.\n\n";
        return;
    }

    constexpr int W_ID           = 10;
    constexpr int W_TIME         = 12;
    constexpr int W_TYPE         = 8;
    constexpr int W_THREAD       = 8;
    constexpr int PAD_ID_TO_TIME = 8;

    const int total_width = W_ID + PAD_ID_TO_TIME + W_TIME + 1 + W_TYPE + 1 + W_THREAD + 2 + (BufferSize - 1);

    os << "ts_store<" << Threads << "," << WorkersPerThread << "," << BufferSize << ">\n";
    os << std::string(total_width, '=') << "\n";

    os << std::right << std::setw(W_ID) << "ID"
       << std::string(PAD_ID_TO_TIME, ' ')
       << std::left  << std::setw(W_TIME)   << "TIME (µs)"
       << " " << std::setw(W_TYPE)   << "TYPE"
       << " " << std::right << std::setw(W_THREAD) << "THREAD"
       << "  PAYLOAD\n";

    os << std::string(total_width, '-') << "\n";

    for (auto id : ids) {
        auto it = rows_.find(id);
        if (it == rows_.end()) {
            os << std::right << std::setw(W_ID) << id << "  <missing>\n";
            continue;
        }
        const auto& r = it->second;

        std::string ts_str   = r.ts_us ? std::to_string(r.ts_us) : "-";
        std::string type_str = r.is_debug ? "Debug" : "Data";
        std::string payload  = r.value;
        payload.resize(BufferSize - 1, '.');

        os << std::right << std::setw(W_ID) << id
           << std::string(PAD_ID_TO_TIME, ' ')
           << std::left  << std::setw(W_TIME)   << ts_str
           << " " << std::setw(W_TYPE)   << type_str
           << " " << std::right << std::setw(W_THREAD) << r.thread_id
           << "  " << payload << "\n";
    }

    os << std::string(total_width, '=') << "\n";
    os << "Total entries: " << rows_.size() << "\n\n";
}