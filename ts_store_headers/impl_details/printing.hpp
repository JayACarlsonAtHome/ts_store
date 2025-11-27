
// Project: ts_store
// File Path: ts_store/ts_store_headers/impl_details/printing.hpp
//

    void print(std::ostream& os = std::cout, int sort_mode = 2) const {
        std::shared_lock lock(data_mtx_);
        auto ids = get_claimed_ids_sorted(sort_mode);
        if (ids.empty()) {
            os << "ts_store<" << Threads << "," << WorkersPerThread << "," << BufferSize << "> is empty.\n\n";
            return;
        }

        size_t w_id   = std::to_string(ids.back()).length();
        size_t w_tid  = std::to_string(Threads - 1).length();
        size_t w_time = 8;
        size_t bufferOffset = 31 + w_id;

        for (uint64_t id : ids) {
            auto it = rows_.find(id);
            if (it == rows_.end()) continue;
            w_tid = std::max(w_tid, std::to_string(it->second.thread_id).length());
            if (it->second.ts_us != 0)
                w_time = std::max(w_time, std::to_string(it->second.ts_us).length());
        }

        w_id = std::max(w_id, size_t(3));
        w_tid = std::max(w_tid, size_t(6));

        os << "ts_store<" << Threads << "," << WorkersPerThread << "," << BufferSize << ">\n";
        os << std::string(BufferSize+bufferOffset, '=') << "\n";

        os << std::right
           << std::setw(w_id)       << "ID"
           << std::setw(w_time + 4) << "TIME"
           << std::left
           << std::setw(8)          << " TYPE"
           << std::right
           << std::setw(w_tid + 4)  << "THREAD"
           << "  PAYLOAD (padded to " << (BufferSize - 1) << " chars)\n";
        os << std::string(BufferSize+bufferOffset, '-') << "\n";

        for (uint64_t id : ids) {
            auto it = rows_.find(id);
            if (it == rows_.end()) {
                os << std::right << std::setw(w_id) << id << " <missing>\n";
                continue;
            }
            const auto& r = it->second;
            std::string ts_str = (r.ts_us != 0) ? std::to_string(r.ts_us) : "-";
            std::string type_str = r.is_debug ? "Debug" : "Data";

            std::string payload = r.value;
            if (payload.length() < BufferSize - 1) {
                payload += std::string((BufferSize - 1) - payload.length(), '.');
            }

            os << std::right
               << std::setw(w_id)       << id
               << std::setw(w_time + 4) << ts_str
               << std::left
               << std::setw(8)          << (" " + type_str)
               << std::right
               << std::setw(w_tid + 4)  << r.thread_id
               << "  " << payload << std::endl;
        }

        os << std::string(BufferSize+bufferOffset, '=') << "\n\n" << std::endl;

    }
