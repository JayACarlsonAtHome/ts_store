// tools/test_cli/main.cpp
//
// C++ CLI tool for running the ts_store test matrix.
//
// Replaces the previous Python and shell scripts for better error handling,
// type safety, and integration with the C++ ecosystem.
//
// Usage:
//   ./ts_test_cli run [--compiler gcc|clang|all] [--disk x7k|10k|ssd] [--params path]
//   ./ts_test_cli summarize [--disk x7k|10k|ssd] [--params path]
//   ./ts_test_cli promote [--all]
//   ./ts_test_cli clean [--results]
//
// The tool uses the same tests/test_params.txt format for compatibility.

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <chrono>
#include <format>

import jac.report;

namespace fs = std::filesystem;

// Simple parser for test_params.txt (KEY=VALUE, ignores # comments, whitespace)
TestParams load_test_params(const fs::path& config_file) {
    TestParams params;
    std::ifstream file(config_file);
    if (!file) {
        return params;
    }

    std::string line;
    while (std::getline(file, line)) {
        // strip comments
        if (auto hash = line.find('#'); hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        // trim
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line.find('=') == std::string::npos) continue;

        auto eq = line.find('=');
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        // trim key/value
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
    TestScaling res;  // default is smoke

    bool is_full = (size == "full" || size == "FULL");

    std::string tnum = test_num_str;
    if (tnum.size() > 3) tnum = tnum.substr(tnum.size() - 3);
    while (tnum.size() < 3) tnum = "0" + tnum;

    if (!is_full) {
        // Heavy tests (005–007): smoke should verify behavior, not stress the machine.
        // Cap at 1000 records per run (threads × events_per_thread).
        if (tnum == "005" || tnum == "006" || tnum == "007") {
            res.threads           = 10;
            res.events_per_thread = 100;
            res.runs              = 1;
        }
        return res;
    }

    if (tnum == "001") {
        res.threads           = 8;
        res.events_per_thread = 64;
        res.runs              = 1;
        res.writer_threads    = 4;
        res.ops_per_thread    = 32;
    } else if (tnum == "002") {
        res.threads           = 16;
        res.events_per_thread = 128;
        res.runs              = 2;
        res.writer_threads    = 6;
        res.ops_per_thread    = 64;
    } else if (tnum == "003") {
        res.threads           = 32;
        res.events_per_thread = 128;
        res.runs              = 3;
        res.writer_threads    = 8;
        res.ops_per_thread    = 80;
    } else if (tnum == "004") {
        res.threads           = 40;
        res.events_per_thread = 200;
        res.runs              = 4;
        res.writer_threads    = 10;
        res.ops_per_thread    = 100;
    } else if (tnum == "005" || tnum == "006" || tnum == "007") {
        // Upgraded: 1M events max per run, 100 threads, 5 runs total
        res.threads           = 100;
        res.events_per_thread = 10000;   // 100 * 10000 = 1M
        res.runs              = 5;
        res.writer_threads    = 100;
        res.ops_per_thread    = 10000;
    } else {
        res.threads           = 100;
        res.events_per_thread = 10000;
        res.runs              = 5;
        res.writer_threads    = 50;
        res.ops_per_thread    = 2000;
    }
    return res;
}

// Match run_all_tests.sh SIZE_LABEL (5-char alignment under 3-char disk dirs).
std::string size_label_from_size(const std::string& size) {
    if (size == "full" || size == "FULL") {
        return "xFull";
    }
    return "Smoke";
}

// Map persist type to the shell runner's log root directory name.
std::string persist_log_dir_name(const std::string& persist) {
    if (persist == "jtext") return "jText_logs";
    if (persist == "sql")   return "sql_logs";
    if (persist == "none")  return "inmem_logs";
    return "binary_logs";
}

// Map e.g. "ts_store_005_TS" -> "TS_STORE_TEST_005_TS" (shell convention).
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

// test-results/OS_003/<disk>/<Smoke|xFull>/  or  test-results/<disk>/  when no OS_ID.
fs::path compute_results_base(const fs::path& project_root,
                              const std::string& os_id,
                              const std::string& disk_type,
                              const std::string& size) {
    fs::path base = project_root / "test-results";
    if (!os_id.empty()) {
        base /= os_id;
    }
    base /= disk_type;
    if (!os_id.empty()) {
        base /= size_label_from_size(size);
    }
    return base;
}

fs::path scenario_log_dir(const fs::path& results_base,
                          const Scenario& scen) {
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

RunResult run_scenario(const Scenario& scen, const fs::path& project_root, const fs::path& log_dir) {
    // Prefer binaries in current directory (e.g. when running from a build dir),
    // fall back to project_root (source tree).
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

    // Pad columns for readability in long runs
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

void print_usage() {
    std::cout << "ts_test_cli - ts_store test matrix runner (C++)\n\n"
              << "Commands:\n"
              << "  run        Run the test matrix\n"
              << "  summarize      Build SQLite + markdown from run_manifest.jtext\n"
              << "  summarize-hub  Regenerate test-summary/README.md index\n"
              << "  promote        Promote summaries to test-summary/\n"
              << "  clean      Clean results\n\n"
              << "Examples:\n"
              << "  ./ts_test_cli run --compiler gcc --disk ssd\n"
              << "  (After ./scripts/build_dual_compilers.sh: cd build-dual/gcc && ./ts_test_cli run --disk ssd)\n"
              << "  ./ts_test_cli run --help\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];

    if (command == "run") {
        // Use a struct + helper to avoid long list of local variables
        struct RunOptions {
            std::string compilerx = "all";
            std::string disk;
            fs::path    params_file{"tests/test_params.txt"};
            bool        dry_run   = false;
            bool        verbosx   = false;
        };

        auto parse_run_opts = [](int ac, char** av) -> RunOptions {
            RunOptions opts;
            for (int i = 2; i < ac; ++i) {
                std::string arg = av[i];
                if (arg == "--compiler" && i + 1 < ac) {
                    opts.compilerx = av[++i];
                } else if (arg == "--disk" && i + 1 < ac) {
                    opts.disk = av[++i];
                } else if (arg == "--params" && i + 1 < ac) {
                    opts.params_file = av[++i];
                } else if (arg == "--dry-run") {
                    opts.dry_run = true;
                } else if (arg == "--verbose" || arg == "-v") {
                    opts.verbosx = true;
                } else if (arg == "--help" || arg == "-h") {
                    std::cout << "run options: --compiler [gcc|clang|all] --disk <type> --params <file> --dry-run -v\n"
                              << "  Tip: run directly from a build dir that contains the ts_store_* test binaries (e.g. build-dual/gcc).\n"
                              << "  The CLI will auto-infer --compiler from the dir name when possible.\n";
                    std::exit(0);
                }
            }
            return opts;
        };

        RunOptions opts = parse_run_opts(argc, argv);

        fs::path launch_dir = fs::current_path();
        std::string exe_path = (argc > 0 ? argv[0] : "");

        // If user didn't pass --compiler explicitly, try to infer from the directory we are in
        // (common after build_dual_compilers.sh: run from build-dual/gcc or build-dual/clang).
        bool compiler_explicit = false;
        for (int i = 2; i < argc; ++i) {
            if (std::string(argv[i]) == "--compiler") { compiler_explicit = true; break; }
        }

        fs::path project_root = find_project_root(launch_dir);

        auto params = load_test_params(project_root / opts.params_file);

        std::string size = params.size;
        std::string disk_type = opts.disk.empty() ? params.disk_type : opts.disk;

        std::vector<std::string> compilers;
        if (!compiler_explicit && opts.compilerx == "all") {
            // Infer single-compiler run when launched from a compiler-specific dual build dir.
            std::string p = launch_dir.string() + " " + exe_path;
            if (p.find("build-dual/gcc") != std::string::npos || launch_dir.filename() == "gcc") {
                compilers = {"gcc"};
                opts.compilerx = "gcc";
            } else if (p.find("build-dual/clang") != std::string::npos || launch_dir.filename() == "clang") {
                compilers = {"clang"};
                opts.compilerx = "clang";
            } else {
                compilers = {"gcc", "clang"};
            }
        } else if (opts.compilerx == "all") {
            compilers = {"gcc", "clang"};
        } else {
            compilers = {opts.compilerx};
        }

        auto get_selected = [](const TestParams& p) -> std::vector<std::string> {
            std::vector<std::string> sel;
            for (int i = 1; i <= 7; ++i) {
                std::string key = std::format("{:03d}", i);
                if (p.selected_tests.count(key) && p.selected_tests.at(key)) {
                    sel.push_back(key);
                }
            }
            if (sel.empty()) {
                for (int i = 1; i <= 7; ++i) sel.push_back(std::format("{:03d}", i));
            }
            return sel;
        };
        std::vector<std::string> selected = get_selected(params);

        fs::path results_base = compute_results_base(project_root, params.os_id, disk_type, size);

        // Padded output for columns
        std::cout << "=== ts_store Test Matrix (C++ CLI) ===\n";
        std::cout << std::format("SIZE={:<6} DISK={:<4}", size, disk_type);
        if (!params.os_id.empty()) {
            std::cout << std::format(" OS_ID={}", params.os_id);
        }
        std::cout << " COMPILERS=";
        for (auto& c : compilers) std::cout << c << " ";
        std::cout << "\nResults: " << results_base.string() << "\n";
        std::cout << "Selected: ";
        for (auto& s : selected) std::cout << s << " ";
        std::cout << "\n\n";

        auto scenarios = build_scenario_list(selected, compilers, size);
        std::cout << "Total scenarios: " << scenarios.size() << "\n\n";

        if (opts.dry_run) {
            std::cout << "DRY-RUN:\n";
            for (const auto& s : scenarios) {
                fs::path log_dir = scenario_log_dir(results_base, s);
                fs::path log_path = log_dir / (s.compiler + "_" + s.persist + "_" + s.output_mode + ".log");
                std::cout << std::format("  {:<22} {:<6} {:<4} {:<6} t={:>3} e={:>5} r={:>2}\n",
                                         s.test, s.persist, s.output_mode, s.compiler,
                                         s.threads, s.events_per_thread, s.runs);
                if (opts.verbosx) {
                    std::cout << "    -> " << log_path.string() << "\n";
                }
            }
            return 0;
        }

        fs::create_directories(results_base);

        std::vector<RunResult> run_results;
        run_results.reserve(scenarios.size());

        int failed = 0;
        for (size_t i = 0; i < scenarios.size(); ++i) {
            const auto& scen = scenarios[i];
            std::cout << "[" << (i+1) << "/" << scenarios.size() << "] ";
            fs::path log_dir = scenario_log_dir(results_base, scen);
            RunResult rr = run_scenario(scen, project_root, log_dir);
            run_results.push_back(rr);
            if (!rr.success) ++failed;
        }

        std::cout << "\n=== Summary ===\n";
        std::cout << std::format("Total:   {:>3}\nFailed:  {:>3}\n", scenarios.size(), failed);

        RunMeta run_meta;
        run_meta.os_id = params.os_id;
        run_meta.disk_type = disk_type;
        run_meta.size_label = size_label_from_size(size);
        {
            std::ostringstream cs;
            for (size_t i = 0; i < compilers.size(); ++i) {
                if (i) cs << ',';
                cs << compilers[i];
            }
            run_meta.compilers_csv = cs.str();
        }
        run_meta.total_scenarios = static_cast<int>(run_results.size());
        run_meta.passed = static_cast<int>(run_results.size()) - failed;
        run_meta.failed = failed;

        std::vector<RunResult> manifest_rows;
        for (const auto& rr : run_results) {
            if (scenario_in_manifest_pilot(rr.scenario)) {
                manifest_rows.push_back(rr);
            }
        }
        if (!manifest_rows.empty()) {
            write_run_manifest_jtext(results_base, run_meta, manifest_rows);
            summarize_results_leaf(results_base, project_root);
        }

        return failed > 0 ? 1 : 0;

    } else if (command == "summarize") {
        fs::path launch_dir = fs::current_path();
        fs::path project_root = find_project_root(launch_dir);

        std::string disk_type;
        fs::path params_file = "tests/test_params.txt";
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--disk" && i + 1 < argc) {
                disk_type = argv[++i];
            } else if (arg == "--params" && i + 1 < argc) {
                params_file = argv[++i];
            } else if (arg == "--help" || arg == "-h") {
                std::cout << "summarize options: --disk <type> --params <file>\n";
                return 0;
            }
        }

        auto params = load_test_params(project_root / params_file);
        if (disk_type.empty()) disk_type = params.disk_type;

        fs::path results_base = compute_results_base(
            project_root, params.os_id, disk_type, params.size);

        if (!summarize_results_leaf(results_base, project_root)) {
            return 1;
        }
        return 0;

    } else if (command == "summarize-hub") {
        fs::path launch_dir = fs::current_path();
        fs::path project_root = find_project_root(launch_dir);
        if (!write_test_summary_hub(project_root)) {
            return 1;
        }
        return 0;

    } else if (command == "promote") {
        // Delegate to shell script for fidelity (or reimplement in C++ later)
        std::string promote_cmd = "./scripts/promote_summaries.sh";
        for (int i = 2; i < argc; ++i) {
            promote_cmd += " ";
            promote_cmd += argv[i];
        }
        std::cout << "Delegating to: " << promote_cmd << "\n";
        return std::system(promote_cmd.c_str());

    } else if (command == "clean") {
        std::cout << "Cleaning test results (C++ stub - expand as needed)...\n";
        // Simple implementation
        fs::path results = fs::current_path() / "test-results";
        if (fs::exists(results)) {
            fs::remove_all(results);
            std::cout << "Removed test-results/\n";
        }
        return 0;

    } else {
        print_usage();
        return 1;
    }
}
