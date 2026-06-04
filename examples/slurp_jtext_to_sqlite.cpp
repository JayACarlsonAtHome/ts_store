#include <beman/ts_store/ts_store_headers/persistence/Sqlite.hpp>
#include <jText.h>   // for reading the jText data files

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

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
            std::string sql((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
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
            if (sec.name != "Data Section") continue;
            for (const auto& entry : sec.entries) {
                if (entry.fields.size() < 6) continue;
                // fields from the writer: thread_id, per_thread..., flags, category, payload, timestamp
                int64_t id = entry.number;
                stmt.bind(
                    id,
                    std::stoll(entry.fields[0]),
                    std::stoll(entry.fields[1]),
                    std::stoll(entry.fields[2], nullptr, 16), // hex flags
                    entry.fields[3],
                    entry.fields[4],
                    std::stoll(entry.fields[5])
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
                if (sec.name != "Data Section") continue;
                for (const auto& entry : sec.entries) {
                    if (entry.fields.empty()) continue;
                    int64_t id = std::stoll(entry.fields[0]);
                    stmt.bind(id);
                    for (size_t i = 1; i < entry.fields.size() && i < 10; ++i) {
                        stmt.bind(std::stoll(entry.fields[i]));  // simplistic, would use better bind in real code
                    }
                    stmt.step();
                    stmt.reset();
                }
            }
            db.commit();
        }
    }

    // Similar for Floats... (omitted for brevity, same pattern)

    std::cout << "Slurped " << base_name << " into " << db_path << "\n";
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