#include "run_manifest.hpp"
#include "types.hpp"

#include <jText.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iostream>
#include <map>
#include <sstream>
#include <format>

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

JTextEntry make_entry(size_t num, const std::vector<std::string>& fields, std::string_view comment = {}) {
    JTextEntry e;
    e.number = num;
    e.delimiter = '#';
    e.level_sep = '|';  // jText writes inter-field separator via level_sep
    e.fields.reserve(fields.size());
    for (const auto& f : fields) {
        e.fields.push_back(f);
    }
    e.comment = std::string(comment);
    return e;
}

std::optional<RunResult> parse_scenario_entry(const JTextEntry& e, const fs::path& results_base) {
    auto fields = split_fields(e);
    if (fields.size() < 8) return std::nullopt;

    RunResult r;
    std::string subdir = fields[0];
    r.scenario.compiler = fields[1];
    r.scenario.persist = fields[2];
    r.scenario.output_mode = fields[3];
    // fields[4] stores total records, not events_per_thread
    const int records = static_cast<int>(std::stoll(fields[4]));
    r.scenario.threads = 1;
    r.scenario.events_per_thread = records;
    r.scenario.runs = 1;
    // recover binary name from subdir
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

    JTextFile jf;
    jf.date = meta.run_utc.empty() ? utc_now_iso() : meta.run_utc;
    jf.purpose = "ts_store test matrix run manifest";
    jf.table_name = "ts_run_manifest";
    jf.filename = "run_manifest.jtext";
    jf.case_mode = CaseMode::Sensitive;

    std::string compilers_csv = merge_compilers_csv(
        load_existing_compilers_csv(jtext_path), meta.compilers_csv);

    JTextSection meta_sec;
    meta_sec.name = "RunMeta";
    meta_sec.entries.push_back(make_entry(
        1,
        {
            meta.os_id,
            meta.disk_type,
            meta.size_label,
            jf.date,
            compilers_csv,
            std::to_string(static_cast<int>(merged.size())),
            std::to_string(passed),
            std::to_string(failed),
        },
        "os_id|disk|size|run_utc|compilers|total|passed|failed"
    ));

    JTextSection scen_sec;
    scen_sec.name = "Scenarios";
    size_t n = 1;
    std::vector<RunResult> sorted;
    sorted.reserve(merged.size());
    for (auto& [_, r] : merged) sorted.push_back(r);
    std::sort(sorted.begin(), sorted.end(), [](const RunResult& a, const RunResult& b) {
        if (a.scenario.test != b.scenario.test) return a.scenario.test < b.scenario.test;
        if (a.scenario.compiler != b.scenario.compiler) return a.scenario.compiler < b.scenario.compiler;
        if (a.scenario.persist != b.scenario.persist) return a.scenario.persist < b.scenario.persist;
        return a.scenario.output_mode < b.scenario.output_mode;
    });

    for (const auto& r : sorted) {
        std::string log_rel;
        if (!r.log_path.empty()) {
            log_rel = fs::relative(r.log_path, results_base).string();
        }
        int records = scenario_record_count(r.scenario);
        scen_sec.entries.push_back(make_entry(
            n++,
            {
                test_to_subdir_name(r.scenario.test),
                r.scenario.compiler,
                r.scenario.persist,
                r.scenario.output_mode,
                std::to_string(records),
                std::format("{:.3f}", r.duration_sec),
                r.success ? "PASS" : "FAIL",
                log_rel,
            },
            "test|compiler|persist|output|records|duration_sec|status|log_relpath"
        ));
    }

    jf.sections = {std::move(meta_sec), std::move(scen_sec)};

    if (auto wr = jf.write(jtext_path.string()); !wr) {
        std::cerr << "ERROR writing manifest: " << wr.error() << "\n";
        return false;
    }

    std::cout << "Wrote manifest: " << jtext_path.string()
              << " (" << merged.size() << " scenarios)\n";
    return true;
}