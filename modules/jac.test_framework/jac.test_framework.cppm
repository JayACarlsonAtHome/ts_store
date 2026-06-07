module;

#include <filesystem>
#include <string>
#include <vector>

export module jac.test_framework;

export import jac.report;

export TestParams load_test_params(const std::filesystem::path& config_file);
export TestScaling get_test_params(const std::string& test_num_str, const std::string& size);
export std::string size_label_from_size(const std::string& size);
export std::filesystem::path compute_results_base(const std::filesystem::path& project_root,
                                                  const std::string& os_id,
                                                  const std::string& disk_type,
                                                  const std::string& size);
export std::filesystem::path scenario_log_dir(const std::filesystem::path& results_base,
                                              const Scenario& scen);
export std::vector<Scenario> build_scenario_list(const std::vector<std::string>& selected,
                                                 const std::vector<std::string>& compilers,
                                                 const std::string& size);
export RunResult run_scenario(const Scenario& scen,
                              const std::filesystem::path& project_root,
                              const std::filesystem::path& log_dir);
export std::filesystem::path find_project_root(const std::filesystem::path& start);
export std::vector<std::string> infer_compilers(const std::string& compiler_opt,
                                                bool compiler_explicit,
                                                const std::filesystem::path& launch_dir,
                                                const std::string& exe_path);
export std::vector<std::string> get_selected_tests(const TestParams& params);