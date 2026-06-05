#include <beman/ts_store/ts_store_headers/persistence/Sqlite.hpp>
#include <jText.h>   // for reading the jText data files

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <format>

namespace fs = std::filesystem;

// Simple slurper: reads the three jText files produced by JTextSplitEventLog
// and inserts them into the matching SQLite tables (using the generated schema).
// Assumes the .sql files have already been used to create the tables, or we exec them here.

void slurp_jtext_to_sqlite(const std::string& base_name, const std::string& db_path) {
    jac::ts_store::Sqlite db(db_path);
    db.exec("PRAGMA synchronous = OFF;");
    db.exec("PRAGMA journal_mode = MEMORY;");

    // Optionally exec the schema files if they exist
    for (const auto& suffix : {"", "_Ints", "_Floats"}) {
        std::string schema = base_name + suffix + ".sql";
        if (fs::exists(schema)) {
            std::ifstream f(schema);
            std::string sql;
            std::string line;
            while (std::getline(f, line)) {
                sql += line + "\n";
            }
            try {
                db.exec(sql);
            } catch (...) {
                // ignore if tables already exist
            }
        }
    }

    // Read main jtext
    {
        JTextFile main_jt;
        if (auto res = main_jt.read_full(base_name + ".jtext"); !res) {
            throw std::runtime_error("Failed to read main jtext: " + res.error());
        }

        auto stmt = db.prepare(
            "INSERT OR IGNORE INTO " + base_name +
            " (id, thread_id, per_thread_event_id, flags_raw, category, payload, timestamp_us) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)");

        db.begin();
        for (const auto& sec : main_jt.sections) {
            for (const auto& entry : sec.entries) {
                if (entry.fields.size() < 6) continue;
                // fields from the writer: thread_id, per_thread..., flags, category, payload, timestamp
                int64_t id = static_cast<int64_t>(entry.number);
                stmt.bind(
                    id,
                    static_cast<int64_t>(std::stoll(entry.fields[0])),
                    static_cast<int64_t>(std::stoll(entry.fields[1])),
                    static_cast<int64_t>(std::stoll(entry.fields[2], nullptr, 16)), // hex flags
                    entry.fields[3],
                    entry.fields[4],
                    static_cast<int64_t>(std::stoll(entry.fields[5]))
                );
                stmt.step();
                stmt.reset();
            }
        }
        db.commit();
    }

    // Read Ints
    std::string ints_file = base_name + "_Ints.jtext";
    if (fs::exists(ints_file)) {
        JTextFile ints_jt;
        if (auto res = ints_jt.read_full(ints_file); !res) {
            std::cerr << "Warning: could not read ints file\n";
        } else {
            // Build dynamic INSERT based on number of int columns from first row or field list
            // For simplicity we assume the schema matches what was written
            auto stmt = db.prepare(
                "INSERT OR IGNORE INTO " + base_name + "_ints (id, int0, int1, int2, int3, int4, int5, int6, int7, int8) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

            db.begin();
            for (const auto& sec : ints_jt.sections) {
                for (const auto& entry : sec.entries) {
                    if (entry.fields.empty()) continue;
                    int64_t id = static_cast<int64_t>(std::stoll(entry.fields[0]));
                    stmt.bind(id);
                    for (size_t i = 1; i < entry.fields.size() && i < 10; ++i) {
                        stmt.bind(static_cast<int64_t>(std::stoll(entry.fields[i])));  // simplistic, would use better bind in real code
                    }
                    stmt.step();
                    stmt.reset();
                }
            }
            db.commit();
        }
    }

    // Floats - same pattern
    std::string floats_file = base_name + "_Floats.jtext";
    if (fs::exists(floats_file)) {
        JTextFile floats_jt;
        if (auto res = floats_jt.read_full(floats_file); !res) {
            std::cerr << "Warning: could not read floats file\n";
        } else {
            auto stmt = db.prepare(
                "INSERT OR IGNORE INTO " + base_name + "_floats (id, dbl0, dbl1, dbl2, dbl3, dbl4, dbl5) "
                "VALUES (?, ?, ?, ?, ?, ?, ?)");

            db.begin();
            for (const auto& sec : floats_jt.sections) {
                for (const auto& entry : sec.entries) {
                    if (entry.fields.empty()) continue;
                    int64_t id = static_cast<int64_t>(std::stoll(entry.fields[0]));
                    stmt.bind(id);
                    for (size_t i = 1; i < entry.fields.size() && i < 7; ++i) {
                        stmt.bind(static_cast<double>(std::stod(entry.fields[i])));
                    }
                    stmt.step();
                    stmt.reset();
                }
            }
            db.commit();
        }
    }

    // Export back to jText with new names (fulltrip) for easy comparison to originals.
    // This is the "back to jText" part of the round trip, using new file names as requested.
    {
        std::string back_base = base_name + "_fulltrip";
        std::ofstream out(back_base + ".jtext");
        out << "//File:    " << back_base + ".jtext\n";
        auto now = std::chrono::system_clock::now();
        auto today = std::chrono::floor<std::chrono::days>(now);
        std::string date_str = std::format("{:%Y-%m-%d}", today);
        out << "//Date:    " << date_str << "\n";
        out << "//Purpose: jText Data File (full roundtrip via SQL ingest)\n";
        out << "//\n";
        out << "=== jText File ===\n";
        out << " 1. #?# " << back_base + ".jtext\n";
        out << " 2. #?# " << date_str << "\n";
        out << " 3. #?# Full roundtrip from ts_store jText logs via SQL\n";
        out << "\n";
        out << "=== Section: " << base_name << " ===\n";
        out << "=== Fields ===\n";
        out << " 1. #/# id/Number/Not Null\n";
        out << " 2. #/# thread_id/Number/Not Null\n";
        out << " 3. #/# per_thread_event_id/Number/Not Null\n";
        out << " 4. #/# flags_raw/String\n";
        out << " 5. #/# category/String\n";
        out << " 6. #/# payload/String\n";
        out << " 7. #/# timestamp_us/Number\n";
        out << "=== Data ===\n";

        auto stmt = db.prepare("SELECT id, thread_id, per_thread_event_id, flags_raw, category, payload, timestamp_us FROM " + base_name + " ORDER BY id");
        while (stmt.step()) {
            int64_t id, thread, per, ts;
            std::string flags, cat, payload;
            stmt.get(id, thread, per, flags, cat, payload, ts);
            out << " 1. #?# " << id << "\n";
            out << " 2. #?# " << thread << "\n";
            out << " 3. #?# " << per << "\n";
            out << " 4. #/# " << flags << "\n";
            out << " 5. #?# " << cat << "\n";
            out << " 6. #?# " << payload << "\n";
            out << " 7. #?# " << ts << "\n\n";
        }

        out << "=== End Data ===\n";
        out << "=== End Section ===\n";
        out << "=== End File ===\n";
        std::cout << "Exported roundtrip back to " << back_base << ".jtext\n";
    }

    std::cout << "Slurped " << base_name << " into " << db_path << " and exported fulltrip jText\n";
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <base_name> <db.sqlite>\n";
        return 1;
    }
    try {
        slurp_jtext_to_sqlite(argv[1], argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}