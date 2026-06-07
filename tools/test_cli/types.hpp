#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace fs = std::filesystem;

struct TestParams {
    std::string size = "smoke";
    std::string disk_type = "ssd";
    std::string os_id;
    std::map<std::string, bool> selected_tests;
};

struct TestScaling {
    int threads           = 5;
    int events_per_thread = 20;
    int runs              = 1;
    int writer_threads    = 5;
    int ops_per_thread    = 20;
};

struct Scenario {
    std::string test;
    std::string persist;
    std::string output_mode;
    std::string compiler;
    int threads           = 5;
    int events_per_thread = 20;
    int runs              = 1;
};

struct RunResult {
    Scenario scenario;
    bool success          = false;
    double duration_sec   = 0.0;
    fs::path log_path;
};

struct RunMeta {
    std::string os_id;
    std::string disk_type;
    std::string size_label;
    std::string run_utc;
    std::string compilers_csv;
    int total_scenarios   = 0;
    int passed            = 0;
    int failed            = 0;
};