#pragma once

#include "types.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

bool scenario_in_manifest_pilot(const Scenario& scen);

int scenario_record_count(const Scenario& scen);

// Merge this run's results into run_manifest.jtext under the results leaf.
bool write_run_manifest_jtext(const fs::path& results_base,
                              const RunMeta& meta,
                              const std::vector<RunResult>& results);

// Slurp jtext → sqlite views → README.md + by_test/*.md
bool summarize_results_leaf(const fs::path& results_base,
                            const fs::path& project_root);

// Scan test-summary/ leaves and write test-summary/README.md hub index.
bool write_test_summary_hub(const fs::path& project_root);