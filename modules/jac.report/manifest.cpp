module;

#include <filesystem>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <format>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

module jac.report;

import jac.jtext.reader;
import jac.jtext.writer;

namespace fs = std::filesystem;

namespace {

constexpr bool kManifestPilotOnlyTest001 = false;

std::string scenario_key(const Scenario& s) {
    return s.test + "|" + s.compiler + "|" + s.persist + "|" + s.output_mode;
}

std::string test_to_subdir_name(const std::string& test) {
    constexpr std::string_view prefix = "ts_store_";
    if (test.starts_with(prefix)) {
        return "TS_STORE_TEST_" + test.substr(prefix.size());
    }
    return test;
}

std::vector<std::string> split_fields(const JTextEntry& e) {
    std::vector<std::string> out;
    out.reserve(e.fields.size());
    for (const auto& f : e.fields) {
        out.push_back(f.value_or(""));
    }
    return out;
}

void write_compact_row(std::ostream& out,
                       size_t num,
                       size_t pad_width,
                       const std::vector<std::string>& fields) {
    write_jtext_line_number(out, num, pad_width);
    out << "#|# ";
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) out << '|';
        out << fields[i];
    }
    out << '\n';
}

std::string jtext_includes_rel(const fs::path& from_dir) {
    fs::path p = fs::absolute(from_dir);
    for (;;) {
        if (fs::exists(p / "tests/jtext_includes")) {
            auto rel = fs::relative(p / "tests/jtext_includes", from_dir);
            std::string s = rel.generic_string();
            if (s.empty() || s == ".") return "tests/jtext_includes";
            return s;
        }
        if (!p.has_parent_path() || p == p.parent_path()) break;
        p = p.parent_path();
    }
    return "../../../../tests/jtext_includes";
}

void write_manifest_section(std::ostream& out,
                            std::string_view section_name,
                            std::string_view fields_jtflds,
                            const std::vector<std::vector<std::string>>& rows) {
    write_light_section_banner(out, section_name);
    write_fields_include_line(out, fields_jtflds);
    const size_t pad_width = jtext_line_pad_width_for_count(rows.size());
    size_t n = 1;
    for (const auto& row : rows) {
        write_compact_row(out, n++, pad_width, row);
    }
    out << '\n';
}

std::optional<RunResult> parse_scenario_entry(const JTextEntry& e, const fs::path& results_base) {
    auto fields = split_fields(e);
    if (fields.size() < 8) return std::nullopt;

    RunResult r;
    std::string subdir = fields[0];
    r.scenario.compiler = fields[1];
    r.scenario.persist = fields[2];
    r.scenario.output_mode = fields[3];
    const int records = static_cast<int>(std::stoll(fields[4]));
    r.scenario.threads = 1;
    r.scenario.events_per_thread = records;
    r.scenario.runs = 1;
    if (subdir.starts_with("TS_STORE_TEST_")) {
        r.scenario.test = "ts_store_" + subdir.substr(std::string("TS_STORE_TEST_").size());
    } else {
        r.scenario.test = subdir;
    }
    r.duration_sec = std::stod(fields[5]);
    r.success = (fields[6] == "PASS" || fields[6] == "pass");
    fs::path log_path = results_base / fields[7];
    if (!fs::exists(log_path)) {
        std::string lower = fields[7];
        for (char& c : lower) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        fs::path alt = results_base / lower;
        if (fs::exists(alt)) log_path = alt;
    }
    r.log_path = log_path;
    return r;
}

bool load_existing_scenarios(const fs::path& jtext_path,
                             const fs::path& results_base,
                             std::map<std::string, RunResult>& out) {
    if (!fs::exists(jtext_path)) return true;

    JTextFile jf;
    if (auto res = jf.read_full(jtext_path.string()); !res) {
        std::cerr << "Warning: could not read existing manifest: " << res.error() << "\n";
        return false;
    }

    for (const auto& sec : jf.sections) {
        if (sec.name != "Scenarios") continue;
        for (const auto& entry : sec.entries) {
            if (auto parsed = parse_scenario_entry(entry, results_base)) {
                out[scenario_key(parsed->scenario)] = *parsed;
            }
        }
    }
    return true;
}

std::string utc_now_iso() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::vector<std::string> split_csv(const std::string& csv) {
    std::vector<std::string> out;
    std::string token;
    for (char c : csv) {
        if (c == ',') {
            if (!token.empty()) out.push_back(token);
            token.clear();
        } else if (!std::isspace(static_cast<unsigned char>(c))) {
            token += c;
        }
    }
    if (!token.empty()) out.push_back(token);
    return out;
}

std::string lower_copy(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string merge_compilers_csv(const std::string& existing, const std::string& incoming) {
    std::vector<std::string> merged;
    for (const auto& c : split_csv(existing)) {
        auto lc = lower_copy(c);
        if (std::find(merged.begin(), merged.end(), lc) == merged.end()) {
            merged.push_back(lc);
        }
    }
    for (const auto& c : split_csv(incoming)) {
        auto lc = lower_copy(c);
        if (std::find(merged.begin(), merged.end(), lc) == merged.end()) {
            merged.push_back(lc);
        }
    }
    std::ostringstream os;
    for (size_t i = 0; i < merged.size(); ++i) {
        if (i) os << ',';
        os << merged[i];
    }
    return os.str();
}

std::string load_existing_compilers_csv(const fs::path& jtext_path) {
    if (!fs::exists(jtext_path)) return {};

    JTextFile jf;
    if (auto res = jf.read_full(jtext_path.string()); !res) return {};

    for (const auto& sec : jf.sections) {
        if (sec.name != "RunMeta" || sec.entries.empty()) continue;
        auto fields = split_fields(sec.entries[0]);
        if (fields.size() >= 5) return fields[4];
    }
    return {};
}

} // namespace

bool scenario_in_manifest_pilot(const Scenario& scen) {
    if (!kManifestPilotOnlyTest001) return true;
    return scen.test.find("ts_store_001_") == 0;
}

int scenario_record_count(const Scenario& scen) {
    if (scen.test == "ts_store_flags") return 1;
    return scen.threads * scen.events_per_thread;
}

bool write_run_manifest_jtext(const fs::path& results_base,
                              const RunMeta& meta,
                              const std::vector<RunResult>& results) {
    fs::create_directories(results_base);
    fs::path jtext_path = results_base / "run_manifest.jtext";

    std::map<std::string, RunResult> merged;
    load_existing_scenarios(jtext_path, results_base, merged);

    for (const auto& r : results) {
        if (!scenario_in_manifest_pilot(r.scenario)) continue;
        merged[scenario_key(r.scenario)] = r;
    }

    int passed = 0;
    int failed = 0;
    for (const auto& [_, r] : merged) {
        if (r.success) ++passed; else ++failed;
    }

    const std::string run_utc = meta.run_utc.empty() ? utc_now_iso() : meta.run_utc;

    std::string compilers_csv = merge_compilers_csv(
        load_existing_compilers_csv(jtext_path), meta.compilers_csv);

    std::vector<RunResult> sorted;
    sorted.reserve(merged.size());
    for (auto& [_, r] : merged) sorted.push_back(r);
    std::sort(sorted.begin(), sorted.end(), [](const RunResult& a, const RunResult& b) {
        if (a.scenario.test != b.scenario.test) return a.scenario.test < b.scenario.test;
        if (a.scenario.compiler != b.scenario.compiler) return a.scenario.compiler < b.scenario.compiler;
        if (a.scenario.persist != b.scenario.persist) return a.scenario.persist < b.scenario.persist;
        return a.scenario.output_mode < b.scenario.output_mode;
    });

    std::vector<std::vector<std::string>> scenario_rows;
    scenario_rows.reserve(sorted.size());
    for (const auto& r : sorted) {
        std::string log_rel;
        if (!r.log_path.empty()) {
            log_rel = fs::relative(r.log_path, results_base).generic_string();
        }
        int records = scenario_record_count(r.scenario);
        scenario_rows.push_back({
            test_to_subdir_name(r.scenario.test),
            r.scenario.compiler,
            r.scenario.persist,
            r.scenario.output_mode,
            std::to_string(records),
            std::format("{:.3f}", r.duration_sec),
            r.success ? "PASS" : "FAIL",
            log_rel,
        });
    }

    const std::string inc_rel = jtext_includes_rel(results_base);
    const std::string runmeta_fields =
        inc_rel + "/run_manifest_runmeta_fields.jtFlds";
    const std::string scenarios_fields =
        inc_rel + "/run_manifest_scenarios_fields.jtFlds";

    std::ofstream out(jtext_path);
    if (!out) {
        std::cerr << "ERROR opening manifest for write: " << jtext_path << "\n";
        return false;
    }

    write_file_comment_header(
        out,
        jtext_path.string(),
        "ts_store test matrix run manifest",
        "type=ts_store table=ts_run_manifest");

    write_jtext_hash_header(
        out,
        run_utc,
        "ts_store test matrix run manifest",
        CaseMode::Sensitive,
        "ts_run_manifest");
    out << '\n';

    write_manifest_section(
        out,
        "RunMeta",
        runmeta_fields,
        {{
            meta.os_id,
            meta.disk_type,
            meta.size_label,
            run_utc,
            compilers_csv,
            std::to_string(static_cast<int>(merged.size())),
            std::to_string(passed),
            std::to_string(failed),
        }});

    write_manifest_section(out, "Scenarios", scenarios_fields, scenario_rows);

    if (!out) {
        std::cerr << "ERROR writing manifest: " << jtext_path << "\n";
        return false;
    }

    std::cout << "Wrote manifest: " << jtext_path.string()
              << " (" << merged.size() << " scenarios)\n";
    return true;
}