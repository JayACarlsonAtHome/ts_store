module;

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

export struct RunMeta {
    std::string os_id;
    std::string disk_type;
    std::string size_label;
    std::string run_utc;
    std::string compilers_csv;
    int total_scenarios = 0;
    int passed          = 0;
    int failed          = 0;
};

export bool scenario_in_manifest_pilot(const Scenario& scen);
export int scenario_record_count(const Scenario& scen);

export bool write_run_manifest_jtext(const fs::path& results_base,
                                     const RunMeta& meta,
                                     const std::vector<RunResult>& results);

export bool summarize_results_leaf(const fs::path& results_base,
                                   const fs::path& project_root);

export bool write_test_summary_hub(const fs::path& project_root);