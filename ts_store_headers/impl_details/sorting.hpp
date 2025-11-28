// Project: ts_store
// File Path: ts_store/ts_store_headers/impl_details/sorting.hpp
//

/*
    std::vector<std::uint64_t> get_claimed_ids_sorted(int mode = 0) const {
        std::shared_lock lock(data_mtx_);
        auto ids = claimed_ids_;

        if (mode == 1) {
            std::sort(ids.begin(), ids.end(), [this](uint64_t a, uint64_t b) {
                auto ita = rows_.find(a); auto itb = rows_.find(b);
                if (ita == rows_.end() || itb == rows_.end()) return false;
                return ita->second.thread_id < itb->second.thread_id;
            });
        }
        else if (mode == 2) {
            std::sort(ids.begin(), ids.end(), [this](uint64_t a, uint64_t b) {
                auto ita = rows_.find(a); auto itb = rows_.find(b);
                if (ita == rows_.end() || itb == rows_.end()) return false;
                return std::strcmp(ita->second.value, itb->second.value) < 0;
            });
        }
        return ids;
    }
*/