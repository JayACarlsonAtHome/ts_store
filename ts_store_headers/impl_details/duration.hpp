
// Project: ts_store
// File Path: ts_store/ts_store_headers/impl_details/duration.hpp
//

void show_duration(const std::string& prefix = "Store") const {
    std::shared_lock lock(data_mtx_);

    uint64_t first_ts = 0, last_ts = 0;
    bool have_first = false, have_last = false;

    for (const auto& pair : rows_) {
        const auto& r = pair.second;
        if (r.ts_us == 0) continue;

        if (!have_first || r.ts_us < first_ts) {
            first_ts = r.ts_us;
            have_first = true;
        }
        if (!have_last || r.ts_us > last_ts) {
            last_ts = r.ts_us;
            have_last = true;
        }
    }

    if (have_first && have_last && last_ts >= first_ts) {
        std::cout << prefix << " duration: " << (last_ts - first_ts) << " µs (first to last)\n";
    }
}