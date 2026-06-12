module;

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

module jac.test_framework;

namespace fs = std::filesystem;

TestParams load_test_params(const fs::path& config_file) {
    TestParams params;
    std::ifstream file(config_file);
    if (!file) {
        return params;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (auto hash = line.find('#'); hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line.find('=') == std::string::npos) continue;

        auto eq = line.find('=');
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        if (key == "SIZE") {
            params.size = value;
        } else if (key == "DISK_TYPE") {
            params.disk_type = value;
        } else if (key == "OS_ID") {
            params.os_id = value;
        } else if (key == "flags") {
            std::string lower_val = value;
            std::transform(lower_val.begin(), lower_val.end(), lower_val.begin(), ::tolower);
            if (lower_val == "x" || lower_val == "1" || lower_val == "true") {
                params.selected_tests["flags"] = true;
            }
        } else if (key.size() == 3 && std::all_of(key.begin(), key.end(), ::isdigit)) {
            std::string lower_val = value;
            std::transform(lower_val.begin(), lower_val.end(), lower_val.begin(), ::tolower);
            if (lower_val == "x" || lower_val == "1" || lower_val == "true") {
                params.selected_tests[key] = true;
            }
        }
    }
    return params;
}

TestScaling get_test_params(const std::string& test_num_str, const std::string& size) {
    TestScaling res;

    bool is_full = (size == "full" || size == "FULL");

    std::string tnum = test_num_str;
    if (tnum.size() > 3) tnum = tnum.substr(tnum.size() - 3);
    while (tnum.size() < 3) tnum = "0" + tnum;

    if (!is_full) {
        if (tnum == "005" || tnum == "006" || tnum == "007") {
            res.threads           = 10;
            res.events_per_thread = 100;
            res.runs              = 1;
        }
        return res;
    }

    // SIZE=full (xFull): progressive scale tuned for ~30 min gcc+clang matrix on x7k.
    // Heavy 005/006/007: 50×2000 = 100k manifest records; 005/007 use 3 runs (300k events).
    if (tnum == "001") {
        res.threads           = 8;
        res.events_per_thread = 64;
        res.runs              = 1;
        res.writer_threads    = 4;
        res.ops_per_thread    = 32;
    } else if (tnum == "002") {
        res.threads           = 12;
        res.events_per_thread = 64;
        res.runs              = 2;
        res.writer_threads    = 6;
        res.ops_per_thread    = 64;
    } else if (tnum == "003") {
        res.threads           = 24;
        res.events_per_thread = 64;
        res.runs              = 3;
        res.writer_threads    = 6;
        res.ops_per_thread    = 64;
    } else if (tnum == "004") {
        res.threads           = 32;
        res.events_per_thread = 80;
        res.runs              = 4;
        res.writer_threads    = 8;
        res.ops_per_thread    = 80;
    } else if (tnum == "005" || tnum == "006" || tnum == "007") {
        res.threads           = 50;
        res.events_per_thread = 2000;
        res.runs              = 3;
        res.writer_threads    = 50;
        res.ops_per_thread    = 2000;
    } else {
        res.threads           = 50;
        res.events_per_thread = 2000;
        res.runs              = 3;
        res.writer_threads    = 25;
        res.ops_per_thread    = 2000;
    }
    return res;
}

std::string size_label_from_size(const std::string& size) {
    if (size == "full" || size == "FULL") {
        return "xFull";
    }
    return "Smoke";
}

namespace {

std::string persist_log_dir_name(const std::string& persist) {
    if (persist == "jtext") return "jText_logs";
    if (persist == "sql")   return "sql_logs";
    if (persist == "none")  return "inmem_logs";
    if (persist == "unit")  return "unit_logs";
    return "binary_logs";
}

std::string test_to_subdir_name(const std::string& test) {
    if (test == "ts_store_flags") {
        return "TS_STORE_TEST_flags";
    }
    constexpr std::string_view prefix = "ts_store_";
    if (test.starts_with(prefix)) {
        return "TS_STORE_TEST_" + test.substr(prefix.size());
    }
    return test;
}

} // namespace

fs::path compute_results_base(const fs::path& project_root,
                              const std::string& os_id,
                              const std::string& compiler,
                              const std::string& disk_type,
                              const std::string& size) {
    fs::path base = project_root / "test-results";
    if (!os_id.empty()) {
        base /= os_id;
    }
    if (!compiler.empty()) {
        base /= compiler;
    }
    base /= disk_type;
    if (!os_id.empty()) {
        base /= size_label_from_size(size);
    }
    return base;
}

fs::path scenario_log_dir(const fs::path& results_base, const Scenario& scen) {
    return results_base
        / persist_log_dir_name(scen.persist)
        / test_to_subdir_name(scen.test);
}

std::vector<Scenario> build_scenario_list(const std::vector<std::string>& selected,
                                          const std::vector<std::string>& compilers,
                                          const std::string& size) {
    std::vector<Scenario> scenarios;
    std::vector<std::string> persist_types = {"binary", "jtext", "sql", "none"};
    std::vector<std::string> output_modes  = {"on", "off"};

    for (const auto& test_base : selected) {
        if (test_base == "flags") {
            for (const auto& compiler : compilers) {
                Scenario s;
                s.test              = "ts_store_flags";
                s.persist           = "unit";
                s.output_mode       = "off";
                s.compiler          = compiler;
                s.threads           = 1;
                s.events_per_thread = 1;
                s.runs              = 1;
                scenarios.push_back(s);
            }
            continue;
        }

        for (const auto& tsxs : std::vector<std::string>{"TS", "XS"}) {
            std::string test = "ts_store_" + test_base + "_" + tsxs;
            TestScaling scaling = get_test_params(test_base, size);

            for (const auto& persist : persist_types) {
                for (const auto& mode : output_modes) {
                    for (const auto& compiler : compilers) {
                        Scenario s;
                        s.test              = test;
                        s.persist           = persist;
                        s.output_mode       = mode;
                        s.compiler          = compiler;
                        s.threads           = scaling.threads;
                        s.events_per_thread = scaling.events_per_thread;
                        s.runs              = scaling.runs;
                        scenarios.push_back(s);
                    }
                }
            }
        }
    }
    return scenarios;
}

RunResult run_scenario(const Scenario& scen,
                       const fs::path& project_root,
                       const fs::path& log_dir) {
    fs::path bin_path = fs::current_path() / scen.test;
    if (!fs::exists(bin_path) || !fs::is_regular_file(bin_path)) {
        bin_path = project_root / scen.test;
    }
    RunResult result;
    result.scenario = scen;

    if (!fs::exists(bin_path) || !fs::is_regular_file(bin_path)) {
        std::cerr << "  ERROR: binary not found: " << bin_path << "\n";
        result.success = false;
        return result;
    }

    fs::create_directories(log_dir);
    fs::path log_path = log_dir / (scen.compiler + "_" + scen.persist + "_" + scen.output_mode + ".log");
    result.log_path = log_path;
    fs::path persist_base = log_dir / "persist";

    std::string color = (scen.output_mode == "on") ? "1" : "0";

    std::string cmd = std::format(
        "\"{}\" --interactive=0 --color={} --persist={} --base-name=\"{}\" "
        "--threads={} --events-per-thread={} --runs={} > \"{}\" 2>&1",
        bin_path.string(),
        color,
        scen.persist,
        persist_base.string(),
        scen.threads,
        scen.events_per_thread,
        scen.runs,
        log_path.string()
    );

    std::cout << std::format("  Running {:<22} | {:<6} | {:<4} (t={:>3}, e={:>5}, r={:>2})\n",
                             scen.test, scen.persist, scen.output_mode,
                             scen.threads, scen.events_per_thread, scen.runs);

    auto start = std::chrono::steady_clock::now();
    int ret = std::system(cmd.c_str());
    auto end = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(end - start).count();

    result.success = (ret == 0);
    result.duration_sec = secs;
    std::cout << std::format("    {:<4} in {:6.1f}s -> {}\n",
                             (result.success ? "PASS" : "FAIL"), secs, log_path.string());

    return result;
}

fs::path find_project_root(const fs::path& start) {
    fs::path project_root = start;
    while (!fs::exists(project_root / "tests/test_params.txt") &&
           !fs::exists(project_root / "CMakeLists.txt") &&
           project_root != project_root.parent_path()) {
        project_root = project_root.parent_path();
    }
    return project_root;
}

std::vector<std::string> infer_compilers(const std::string& compiler_opt,
                                         bool compiler_explicit,
                                         const fs::path& launch_dir,
                                         const std::string& exe_path) {
    if (!compiler_explicit && compiler_opt == "all") {
        std::string p = launch_dir.string() + " " + exe_path;
        const std::string dir = launch_dir.filename().string();
        if (p.find("build-seq") != std::string::npos) {
            if (dir.find("Clang") != std::string::npos || dir.find("clang") != std::string::npos) {
                return {"clang"};
            }
            if (dir.find("GCC") != std::string::npos || dir.find("gcc") != std::string::npos) {
                return {"gcc"};
            }
        }
        if (launch_dir.filename() == "gcc") {
            return {"gcc"};
        }
        if (launch_dir.filename() == "clang") {
            return {"clang"};
        }
        return {"gcc", "clang"};
    }
    if (compiler_opt == "all") {
        return {"gcc", "clang"};
    }
    return {compiler_opt};
}

std::vector<std::string> get_selected_tests(const TestParams& params) {
    std::vector<std::string> sel;
    for (int i = 1; i <= 7; ++i) {
        std::string key = std::format("{:03d}", i);
        if (params.selected_tests.count(key) && params.selected_tests.at(key)) {
            sel.push_back(key);
        }
    }
    if (params.selected_tests.count("flags") && params.selected_tests.at("flags")) {
        sel.push_back("flags");
    }
    if (sel.empty()) {
        for (int i = 1; i <= 7; ++i) sel.push_back(std::format("{:03d}", i));
        sel.push_back("flags");
    }
    return sel;
}