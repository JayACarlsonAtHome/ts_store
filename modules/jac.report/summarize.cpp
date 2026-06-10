module;

#include <filesystem>

#include <algorithm>
#include <fstream>
#include <format>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <vector>

module jac.report;

import jac.jtext.reader;
import jac.qlite;

namespace fs = std::filesystem;

namespace {

struct LeafMeta {
    std::string os_id;
    std::string disk;
    std::string size_label;
    std::string run_utc;
    std::string compilers;
    int total = 0;
    int passed = 0;
    int failed = 0;
};

struct ScenarioRow {
    std::string test;
    std::string compiler;
    std::string persist;
    std::string output_mode;
    int records = 0;
    double duration_sec = 0.0;
    std::string status;
    std::string log_relpath;
};

std::vector<std::string> split_fields(const JTextEntry& e) {
    std::vector<std::string> out;
    for (const auto& f : e.fields) out.push_back(f.value_or(""));
    return out;
}

bool load_manifest(const fs::path& jtext_path, LeafMeta& meta, std::vector<ScenarioRow>& rows) {
    if (!fs::exists(jtext_path)) {
        std::cerr << "No manifest: " << jtext_path << "\n";
        return false;
    }

    JTextFile jf;
    if (auto res = jf.read_full(jtext_path.string()); !res) {
        std::cerr << "Failed to read manifest: " << res.error() << "\n";
        return false;
    }

    for (const auto& sec : jf.sections) {
        if (sec.name == "RunMeta" && !sec.entries.empty()) {
            auto f = split_fields(sec.entries[0]);
            if (f.size() >= 8) {
                meta.os_id = f[0];
                meta.disk = f[1];
                meta.size_label = f[2];
                meta.run_utc = f[3];
                meta.compilers = f[4];
                meta.total = std::stoi(f[5]);
                meta.passed = std::stoi(f[6]);
                meta.failed = std::stoi(f[7]);
            }
        } else if (sec.name == "Scenarios") {
            for (const auto& e : sec.entries) {
                auto f = split_fields(e);
                if (f.size() < 8) continue;
                ScenarioRow r;
                r.test = f[0];
                r.compiler = f[1];
                r.persist = f[2];
                r.output_mode = f[3];
                r.records = std::stoi(f[4]);
                r.duration_sec = std::stod(f[5]);
                r.status = f[6];
                r.log_relpath = f[7];
                rows.push_back(std::move(r));
            }
        }
    }
    return true;
}

void slurp_to_sqlite(const fs::path& db_path, const LeafMeta& meta, const std::vector<ScenarioRow>& rows) {
    if (fs::exists(db_path)) fs::remove(db_path);

    Sqlite db(db_path.string());
    db.exec("PRAGMA journal_mode = WAL;");

    db.exec(R"(
        CREATE TABLE run_meta (
            os_id TEXT, disk TEXT, size_label TEXT, run_utc TEXT,
            compilers TEXT, total INTEGER, passed INTEGER, failed INTEGER
        );
        CREATE TABLE scenarios (
            test TEXT, compiler TEXT, persist TEXT, output_mode TEXT,
            records INTEGER, duration_sec REAL, status TEXT, log_relpath TEXT
        );
    )");

    auto ins_meta = db.prepare(
        "INSERT INTO run_meta VALUES (?,?,?,?,?,?,?,?)");
    ins_meta.bind(meta.os_id, meta.disk, meta.size_label, meta.run_utc,
                  meta.compilers,
                  static_cast<int64_t>(meta.total),
                  static_cast<int64_t>(meta.passed),
                  static_cast<int64_t>(meta.failed));
    ins_meta.step();

    auto ins = db.prepare(
        "INSERT INTO scenarios VALUES (?,?,?,?,?,?,?,?)");
    db.begin();
    for (const auto& r : rows) {
        ins.bind(r.test, r.compiler, r.persist, r.output_mode,
                 static_cast<int64_t>(r.records), r.duration_sec, r.status, r.log_relpath);
        ins.step();
        ins.reset();
    }
    db.commit();

    db.exec(R"(
        CREATE VIEW v_by_test AS
          SELECT test, compiler, persist, output_mode, records,
                 duration_sec, status, log_relpath
          FROM scenarios
          ORDER BY test, compiler, persist, output_mode;

        CREATE VIEW v_leaf_summary AS
          SELECT test,
                 COUNT(*) AS scenarios,
                 SUM(CASE WHEN status = 'PASS' THEN 1 ELSE 0 END) AS passed,
                 SUM(CASE WHEN status != 'PASS' THEN 1 ELSE 0 END) AS failed
          FROM scenarios
          GROUP BY test
          ORDER BY test;

        CREATE VIEW v_failures AS
          SELECT * FROM scenarios WHERE status != 'PASS';
    )");
}

void write_test_page(const fs::path& path,
                     const std::string& test_name,
                     const std::vector<ScenarioRow>& rows,
                     const fs::path& results_base) {
    std::ofstream out(path);
    out << "# " << test_name << "\n\n";
    out << "| Compiler | Persist | Output | Records | Duration | Status | Log |\n";
    out << "|----------|---------|--------|---------|----------|--------|-----|\n";

    for (const auto& r : rows) {
        if (r.test != test_name) continue;
        std::string log_link = r.log_relpath;
        if (!log_link.empty()) {
            // by_test/ is one level below the leaf; logs live at <leaf>/<log_relpath>
            log_link = std::format("[log](../{})", r.log_relpath);
        }
        out << std::format("| {} | {} | {} | {} | {:.2f}s | {} | {} |\n",
                           r.compiler, r.persist, r.output_mode,
                           r.records, r.duration_sec, r.status, log_link);
    }
    out << "\n";
}

void write_leaf_readme(const fs::path& path,
                       const LeafMeta& meta,
                       const std::vector<ScenarioRow>& rows,
                       const fs::path& rel_from_repo) {
    std::set<std::string> tests;
    for (const auto& r : rows) tests.insert(r.test);

    std::ofstream out(path);
    out << "# Test Results — " << meta.os_id << " / " << meta.compilers << " / "
        << meta.disk << " / " << meta.size_label << "\n\n";
    out << "**Run (UTC):** " << meta.run_utc << "  \n";
    out << "**Compilers:** " << meta.compilers << "  \n";
    out << "**Scenarios:** " << meta.passed << "/" << meta.total << " passed";
    if (meta.failed > 0) out << " (**" << meta.failed << " failed**)";
    out << "  \n";
    out << "**Manifest:** [run_manifest.jtext](run_manifest.jtext)  \n\n";
    out << "## Tests\n\n";
    out << "| Test | Scenarios | Passed | Failed | Detail |\n";
    out << "|------|-----------|--------|--------|--------|\n";

    std::map<std::string, std::pair<int,int>> tallies;
    for (const auto& r : rows) {
        auto& t = tallies[r.test];
        ++t.first;
        if (r.status == "PASS") ++t.second;
    }
    for (const auto& tname : tests) {
        int n = tallies[tname].first;
        int p = tallies[tname].second;
        int f = n - p;
        out << std::format("| {} | {} | {} | {} | [by_test/{}.md](by_test/{}.md) |\n",
                           tname, n, p, f, tname, tname);
    }
    out << "\n";
    out << "Raw logs live under `test-results/" << rel_from_repo.string() << "/` (gitignored).\n";
}

struct HubLeaf {
    std::string rel_path;
    std::string os_id;
    std::string compiler;
    std::string disk;
    std::string size_label;
    std::string run_utc;
    std::string compilers;
    int total = 0;
    int passed = 0;
    int failed = 0;
    bool has_manifest = false;
};

std::optional<HubLeaf> hub_leaf_from_readme(const fs::path& readme_path,
                                            const fs::path& test_summary_root) {
    fs::path leaf_dir = readme_path.parent_path();
    if (leaf_dir.filename() == "by_test") return std::nullopt;

    std::string rel = fs::relative(leaf_dir, test_summary_root).string();
    if (rel.empty() || rel == ".") return std::nullopt;

    HubLeaf h;
    h.rel_path = rel;
    auto parts = std::vector<std::string>();
    for (auto it = fs::relative(leaf_dir, test_summary_root); !it.empty(); it = it.parent_path()) {
        if (it == "." || it == it.parent_path()) break;
        parts.push_back(it.filename().string());
    }
    std::reverse(parts.begin(), parts.end());

    if (parts.size() >= 4) {
        h.os_id = parts[0];
        h.compiler = parts[1];
        h.disk = parts[2];
        h.size_label = parts[3];
    } else if (parts.size() == 3) {
        h.os_id = parts[0];
        h.disk = parts[1];
        h.size_label = parts[2];
    } else if (parts.size() == 1) {
        h.os_id = "—";
        h.disk = parts[0];
        h.size_label = "—";
    } else if (parts.size() == 2) {
        h.os_id = "—";
        h.disk = parts[0];
        h.size_label = parts[1];
    }

    fs::path manifest = leaf_dir / "run_manifest.jtext";
    if (fs::exists(manifest)) {
        LeafMeta meta;
        std::vector<ScenarioRow> rows;
        if (load_manifest(manifest, meta, rows)) {
            h.has_manifest = true;
            if (!meta.os_id.empty()) h.os_id = meta.os_id;
            if (!meta.disk.empty()) h.disk = meta.disk;
            if (!meta.size_label.empty()) h.size_label = meta.size_label;
            h.run_utc = meta.run_utc;
            h.compilers = meta.compilers;
            if (h.compiler.empty() && !meta.compilers.empty()) {
                h.compiler = meta.compilers;
            }
            h.total = meta.total;
            h.passed = meta.passed;
            h.failed = meta.failed;
        }
    }
    return h;
}

void write_hub_readme(const fs::path& hub_path, const std::vector<HubLeaf>& leaves) {
    std::ofstream out(hub_path);
    out << "# Test Summary Hub\n\n";
    out << "Committed lightweight results promoted from `test-results/`. "
           "Each leaf links to a per-run README with per-test detail pages under `by_test/`.\n\n";
    out << "| OS | Compiler | Disk | Size | Scenarios | Run (UTC) | Detail |\n";
    out << "|----|----------|------|------|-----------|-----------|--------|\n";

    for (const auto& h : leaves) {
        std::string scenarios = h.has_manifest
            ? std::format("{}/{}", format_locale_int(h.passed), format_locale_int(h.total))
            : "—";
        if (h.has_manifest && h.failed > 0) {
            scenarios += std::format(" (**{} failed**)", format_locale_int(h.failed));
        }
        std::string run_utc = h.run_utc.empty() ? "—" : h.run_utc;
        std::string compiler = h.compiler.empty()
            ? (h.compilers.empty() ? "—" : h.compilers)
            : h.compiler;
        out << std::format("| {} | {} | {} | {} | {} | {} | [README]({}/README.md) |\n",
                           h.os_id, compiler, h.disk, h.size_label,
                           scenarios, run_utc, h.rel_path);
    }
    out << "\n";
    out << "Regenerate: `./scripts/promote_summaries.sh --all` (updates hub automatically).\n";
}

bool write_test_summary_hub_impl(const fs::path& project_root) {
    fs::path test_summary_root = project_root / "test-summary";
    if (!fs::exists(test_summary_root)) {
        std::cerr << "No test-summary/ directory at " << test_summary_root << "\n";
        return false;
    }

    std::vector<HubLeaf> leaves;
    for (const auto& entry : fs::recursive_directory_iterator(test_summary_root)) {
        if (!entry.is_regular_file() || entry.path().filename() != "README.md") continue;
        if (entry.path() == test_summary_root / "README.md") continue;
        if (auto leaf = hub_leaf_from_readme(entry.path(), test_summary_root)) {
            leaves.push_back(std::move(*leaf));
        }
    }

    if (leaves.empty()) {
        std::cerr << "No leaf README.md files under test-summary/\n";
        return false;
    }

    std::sort(leaves.begin(), leaves.end(), [](const HubLeaf& a, const HubLeaf& b) {
        if (a.os_id != b.os_id) return a.os_id < b.os_id;
        if (a.compiler != b.compiler) return a.compiler < b.compiler;
        if (a.disk != b.disk) return a.disk < b.disk;
        return a.size_label < b.size_label;
    });

    fs::path hub_path = test_summary_root / "README.md";
    write_hub_readme(hub_path, leaves);
    std::cout << "Wrote hub: " << hub_path.string() << " (" << leaves.size() << " leaves)\n";
    return true;
}

} // namespace

bool summarize_results_leaf(const fs::path& results_base,
                            const fs::path& project_root) {
    fs::path jtext_path = results_base / "run_manifest.jtext";
    LeafMeta meta;
    std::vector<ScenarioRow> rows;
    if (!load_manifest(jtext_path, meta, rows)) return false;

    fs::path db_path = results_base / "run_manifest.db";
    slurp_to_sqlite(db_path, meta, rows);
    std::cout << "Wrote SQLite: " << db_path.string() << " (views: v_by_test, v_leaf_summary, v_failures)\n";

    fs::path by_test_dir = results_base / "by_test";
    fs::create_directories(by_test_dir);

    std::set<std::string> tests;
    for (const auto& r : rows) tests.insert(r.test);

    for (const auto& tname : tests) {
        fs::path page = by_test_dir / (tname + ".md");
        write_test_page(page, tname, rows, results_base);
        std::cout << "Wrote: " << page.string() << "\n";
    }

    fs::path rel = fs::relative(results_base, project_root / "test-results");
    fs::path readme = results_base / "README.md";
    write_leaf_readme(readme, meta, rows, rel);
    std::cout << "Wrote: " << readme.string() << "\n";

    return true;
}

bool write_test_summary_hub(const fs::path& project_root) {
    return write_test_summary_hub_impl(project_root);
}