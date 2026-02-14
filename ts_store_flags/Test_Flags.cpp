//ts_store_flags/Test_Flags.cpp
// Standalone unit test for TsStoreFlags — completely independent of ts_store

#include "../ts_store_headers/ts_store_flags.hpp"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

int main() {
    std::cout << "Running TsStoreFlags standalone tests...\n\n";

    TsStoreFlags flags;
    flags.init_event_flag_descriptions(flags);  // Populate descriptions

    // Test individual non-severity flags
    flags.set_log_console();
    assert(flags.is_log_console());
    assert(flags.get_set_flags().size() == 1);
    flags.clear_log_console();
    assert(!flags.is_log_console());

    flags.set_keeper_record();
    flags.set_database_entry();
    flags.set_send_network();
    flags.set_hot_cache_hint();
    flags.set_is_result();
    assert(flags.is_keeper_record() && flags.is_database_entry() &&
           flags.is_send_network() && flags.is_hot_cache_hint() &&
           flags.is_result());

    // Test HasData and IsExplicitNull separately (often auto-set)
    flags.set_has_data();
    assert(flags.has_data());
    flags.set_is_explicit_null();
    assert(flags.is_explicit_null());
    flags.clear_has_data();
    flags.clear_is_explicit_null();

    // Test severity handling
    flags.set_severity(TsStoreFlags::Severity::NotSet);
    assert(flags.get_severity() == TsStoreFlags::Severity::NotSet);
    assert(flags.get_severity_string() == "Not Set");

    flags.set_severity(TsStoreFlags::Severity::Critical);
    assert(flags.get_severity() == TsStoreFlags::Severity::Critical);
    assert(flags.get_severity_string() == "Critical");

    flags.set_severity_index(3);  // Info
    assert(flags.get_severity_string() == "Info");

    // Test to_string() and get_set_flags()
    flags.set_log_console();
    flags.set_keeper_record();
    flags.set_has_data();
    flags.set_severity(TsStoreFlags::Severity::Error);

    std::vector<std::string> set_flags = flags.get_set_flags();
    assert(set_flags.size() == 3);
    std::string expected_to_string = "LogConsole, KeeperRecord, HasData | Severity: Error";
    assert(flags.to_string() == expected_to_string);

    // Test serialization round-trip
    auto bytes = flags.to_bytes();
    TsStoreFlags flags2;
    init_event_flag_descriptions(flags2);
    flags2.from_bytes(bytes);

    assert(flags2.to_string() == flags.to_string());
    assert(flags2.get_severity() == flags.get_severity());
    assert(flags2.get_set_flags() == flags.get_set_flags());

    std::cout << "All TsStoreFlags tests PASSED!\n";
    return 0;
}
