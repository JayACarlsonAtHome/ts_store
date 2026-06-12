// tools/test_cli/main.cpp
//
// Thin CLI entry point for the ts_store test matrix.
// Runner logic: jac.test_framework. Reporting: jac.report (re-exported).

#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

import jac.test_framework;

namespace fs = std::filesystem;

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
              << "  (After ./scripts/Build: run from build-seq/<platform>-<compiler>/)\n"
              << "  ./ts_test_cli run --help\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];

    if (command == "run") {
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
                              << "  Tip: run from a build dir with ts_store_* binaries (e.g. build-seq/<platform>-<compiler>).\n"
                              << "  The CLI will auto-infer --compiler from the dir name when possible.\n";
                    std::exit(0);
                }
            }
            return opts;
        };

        RunOptions opts = parse_run_opts(argc, argv);

        fs::path launch_dir = fs::current_path();
        std::string exe_path = (argc > 0 ? argv[0] : "");

        bool compiler_explicit = false;
        for (int i = 2; i < argc; ++i) {
            if (std::string(argv[i]) == "--compiler") { compiler_explicit = true; break; }
        }

        fs::path project_root = find_project_root(launch_dir);
        auto params = load_test_params(project_root / opts.params_file);

        std::string size = params.size;
        std::string disk_type = opts.disk.empty() ? params.disk_type : opts.disk;

        std::vector<std::string> compilers = infer_compilers(
            opts.compilerx, compiler_explicit, launch_dir, exe_path);

        std::vector<std::string> selected = get_selected_tests(params);
        auto scenarios = build_scenario_list(selected, compilers, size);

        std::cout << "=== ts_store Test Matrix (C++ CLI) ===\n";
        std::cout << std::format("SIZE={:<6} DISK={:<4}", size, disk_type);
        if (!params.os_id.empty()) {
            std::cout << std::format(" OS_ID={}", params.os_id);
        }
        std::cout << " COMPILERS=";
        for (auto& c : compilers) std::cout << c << " ";
        std::cout << "\nSelected: ";
        for (auto& s : selected) std::cout << s << " ";
        std::cout << "\nTotal scenarios: "
                  << format_locale_int(static_cast<std::uint64_t>(scenarios.size())) << "\n\n";

        if (opts.dry_run) {
            std::cout << "DRY-RUN:\n";
            for (const auto& s : scenarios) {
                fs::path results_base = compute_results_base(
                    project_root, params.os_id, s.compiler, disk_type, size);
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

        int failed = 0;
        int scenario_index = 0;

        for (const auto& compiler : compilers) {
            fs::path results_base = compute_results_base(
                project_root, params.os_id, compiler, disk_type, size);
            std::cout << "=== Compiler: " << compiler << " ===\n";
            std::cout << "Results: " << results_base.string() << "\n\n";
            fs::create_directories(results_base);

            HostInfo host_info = collect_host_info();
            write_os_info_txt(results_base, host_info, params.os_id);

            std::vector<RunResult> run_results;
            for (const auto& scen : scenarios) {
                if (scen.compiler != compiler) continue;
                ++scenario_index;
                std::cout << "[" << format_locale_int(scenario_index) << "/"
                          << format_locale_int(static_cast<std::uint64_t>(scenarios.size()))
                          << "] ";
                fs::path log_dir = scenario_log_dir(results_base, scen);
                RunResult rr = run_scenario(scen, project_root, log_dir);
                run_results.push_back(rr);
                if (!rr.success) ++failed;
            }

            if (run_results.empty()) continue;

            RunMeta run_meta;
            run_meta.os_id = params.os_id;
            run_meta.disk_type = disk_type;
            run_meta.size_label = size_label_from_size(size);
            run_meta.compilers_csv = compiler;
            run_meta.total_scenarios = static_cast<int>(run_results.size());
            run_meta.passed = 0;
            for (const auto& rr : run_results) {
                if (rr.success) ++run_meta.passed;
            }
            run_meta.failed = run_meta.total_scenarios - run_meta.passed;
            run_meta.host = host_info;

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
            std::cout << "\n";
        }

        std::cout << "=== Summary ===\n";
        std::cout << std::format("Total:   {}\nFailed:  {}\n",
                                 format_locale_int(static_cast<std::uint64_t>(scenarios.size())),
                                 format_locale_int(failed));

        return failed > 0 ? 1 : 0;

    } else if (command == "summarize") {
        fs::path launch_dir = fs::current_path();
        fs::path project_root = find_project_root(launch_dir);

        std::string disk_type;
        std::string compiler;
        fs::path params_file = "tests/test_params.txt";
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--disk" && i + 1 < argc) {
                disk_type = argv[++i];
            } else if (arg == "--compiler" && i + 1 < argc) {
                compiler = argv[++i];
            } else if (arg == "--params" && i + 1 < argc) {
                params_file = argv[++i];
            } else if (arg == "--help" || arg == "-h") {
                std::cout << "summarize options: --disk <type> --compiler <gcc|clang> --params <file>\n";
                return 0;
            }
        }

        auto params = load_test_params(project_root / params_file);
        if (disk_type.empty()) disk_type = params.disk_type;

        fs::path results_base = compute_results_base(
            project_root, params.os_id, compiler, disk_type, params.size);

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
        std::string promote_cmd = "./scripts/promote_summaries.sh";
        for (int i = 2; i < argc; ++i) {
            promote_cmd += " ";
            promote_cmd += argv[i];
        }
        std::cout << "Delegating to: " << promote_cmd << "\n";
        return std::system(promote_cmd.c_str());

    } else if (command == "clean") {
        std::cout << "Cleaning test results (C++ stub - expand as needed)...\n";
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