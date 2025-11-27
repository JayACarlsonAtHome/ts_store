
// Project: ts_store
// File Path: ts_store/ts_store_headers/impl_details/duration.hpp
//

void show_duration(const std::string& prefix = "Store") const {
        if (!useTS_ && !std::any_of(claimed_ids_.begin(), claimed_ids_.end(),
            [this](uint64_t id) { auto it = rows_.find(id); return it != rows_.end() && it->second.ts_us != 0; })) {
            return;
            }

        std::shared_lock lock(data_mtx_);

        uint64_t first_ts = 0, last_ts = 0;
        bool have_first = false, have_last = false;

        for (auto id : claimed_ids_) {
            auto it = rows_.find(id);
            if (it == rows_.end() || it->second.ts_us == 0) continue;   // ← fixed line

            if (!have_first) {
                first_ts = it->second.ts_us;
                have_first = true;
            }
            last_ts = it->second.ts_us;
            have_last = true;
        }

        if (have_first && have_last && last_ts >= first_ts) {
            std::cout << prefix << " duration: " << (last_ts - first_ts) << " µs (first to last)\n";
        }
    }
