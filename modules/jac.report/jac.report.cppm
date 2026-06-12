module;

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

export module jac.report;

namespace fs = std::filesystem;

export struct TestParams {
    std::string size = "smoke";
    std::string disk_type = "ssd";
    std::string os_id;
    std::map<std::string, bool> selected_tests;
};

export struct TestScaling {
    int threads           = 5;
    int events_per_thread = 20;
    int runs              = 1;
    int writer_threads    = 5;
    int ops_per_thread    = 20;
};

export struct Scenario {
    std::string test;
    std::string persist;
    std::string output_mode;
    std::string compiler;
    int threads           = 5;
    int events_per_thread = 20;
    int runs              = 1;
};

export struct RunResult {
    Scenario scenario;
    bool success        = false;
    double duration_sec = 0.0;
    fs::path log_path;
};

export struct HostInfo {
    std::string hostname;
    std::string os_pretty;
    std::string cpu_model;
    int logical_cores   = 0;
    int physical_cores  = 0;
    std::uint64_t ram_total_mib = 0;
    std::uint64_t ram_avail_mib = 0;
    int cpu_mhz_max     = 0;
    std::string arch;
};

export struct RunMeta {
    std::string os_id;
    std::string disk_type;
    std::string size_label;
    std::string run_utc;
    std::string compilers_csv;
    int total_scenarios = 0;
    int passed          = 0;
    int failed          = 0;
    HostInfo host;
};

export HostInfo collect_host_info();
export bool write_os_info_txt(const fs::path& results_base,
                              const HostInfo& host,
                              const std::string& os_id);
export std::string host_info_summary_line(const HostInfo& host);

export bool scenario_in_manifest_pilot(const Scenario& scen);
export int scenario_record_count(const Scenario& scen);

export bool write_run_manifest_jtext(const fs::path& results_base,
                                     const RunMeta& meta,
                                     const std::vector<RunResult>& results);

export bool summarize_results_leaf(const fs::path& results_base,
                                   const fs::path& project_root);

export bool write_test_summary_hub(const fs::path& project_root);

export std::string format_locale_int(std::int64_t value);
export std::string format_locale_int(std::uint64_t value);
export std::string format_locale_int(int value);