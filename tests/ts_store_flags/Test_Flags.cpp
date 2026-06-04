//tests/ts_store_flags/Test_Flags.cpp
// Standalone unit test for TsStoreFlags — completely independent of other ts_store tests

#include "../../include/beman/ts_store/ts_store_headers/ts_store_flags.hpp"
#include "../../include/beman/ts_store/ts_store_headers/impl_details/test_options.hpp"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <concepts>
#include <type_traits>

int main(int argc, char** argv) {
    auto _opts = jac::ts_store::inline_v001::parse_test_options(argc, argv);
    (void)_opts;  // flags test does not use persist/double-buffer; parse for CLI compatibility with runner
    std::cout << "Running TsStoreFlags standalone tests...\n\n";

    TsStoreFlags flags(0);

    // Test user flags
    flags.set(TsStoreFlags::UserFlag::DatabaseEntry);
    flags.set(TsStoreFlags::UserFlag::HotCacheHint);
    flags.set(TsStoreFlags::UserFlag::IsExplicitNull);
    flags.set(TsStoreFlags::UserFlag::IsResult);
    flags.set(TsStoreFlags::UserFlag::KeeperRecord);
    flags.set(TsStoreFlags::UserFlag::LogConsole);
    flags.set(TsStoreFlags::UserFlag::SendNetwork);

    assert(flags.is_set(TsStoreFlags::UserFlag::DatabaseEntry));
    assert(flags.is_set(TsStoreFlags::UserFlag::HotCacheHint));
    assert(flags.is_set(TsStoreFlags::UserFlag::IsExplicitNull));
    assert(flags.is_set(TsStoreFlags::UserFlag::IsResult));
    assert(flags.is_set(TsStoreFlags::UserFlag::KeeperRecord));
    assert(flags.is_set(TsStoreFlags::UserFlag::LogConsole));
    assert(flags.is_set(TsStoreFlags::UserFlag::SendNetwork));

    flags.clear(TsStoreFlags::UserFlag::DatabaseEntry);
    flags.clear(TsStoreFlags::UserFlag::HotCacheHint);
    flags.clear(TsStoreFlags::UserFlag::IsExplicitNull);
    flags.clear(TsStoreFlags::UserFlag::IsResult);
    flags.clear(TsStoreFlags::UserFlag::KeeperRecord);
    flags.clear(TsStoreFlags::UserFlag::LogConsole);
    flags.clear(TsStoreFlags::UserFlag::SendNetwork);

    assert(!flags.is_set(TsStoreFlags::UserFlag::DatabaseEntry));
    assert(!flags.is_set(TsStoreFlags::UserFlag::HotCacheHint));
    assert(!flags.is_set(TsStoreFlags::UserFlag::IsExplicitNull));
    assert(!flags.is_set(TsStoreFlags::UserFlag::IsResult));
    assert(!flags.is_set(TsStoreFlags::UserFlag::KeeperRecord));
    assert(!flags.is_set(TsStoreFlags::UserFlag::LogConsole));
    assert(!flags.is_set(TsStoreFlags::UserFlag::SendNetwork));

    // Test internal flags
    flags.set(TsStoreFlags::InternalFlag::HasData);
    flags.set(TsStoreFlags::InternalFlag::IsInvalid);

    assert(flags.is_set(TsStoreFlags::InternalFlag::HasData));
    assert(flags.is_set(TsStoreFlags::InternalFlag::IsInvalid));

    flags.clear(TsStoreFlags::InternalFlag::HasData);
    flags.clear(TsStoreFlags::InternalFlag::IsInvalid);

    assert(!flags.is_set(TsStoreFlags::InternalFlag::HasData));
    assert(!flags.is_set(TsStoreFlags::InternalFlag::IsInvalid));

    // Test metric flags (bits 18-21)
    flags.set(TsStoreFlags::MetricFlag::HasIntData);
    flags.set(TsStoreFlags::MetricFlag::HasIntStats);
    flags.set(TsStoreFlags::MetricFlag::HasDblData);
    flags.set(TsStoreFlags::MetricFlag::HasDblStats);

    assert(flags.is_set(TsStoreFlags::MetricFlag::HasIntData));
    assert(flags.is_set(TsStoreFlags::MetricFlag::HasIntStats));
    assert(flags.is_set(TsStoreFlags::MetricFlag::HasDblData));
    assert(flags.is_set(TsStoreFlags::MetricFlag::HasDblStats));

    flags.clear(TsStoreFlags::MetricFlag::HasIntData);
    flags.clear(TsStoreFlags::MetricFlag::HasIntStats);
    flags.clear(TsStoreFlags::MetricFlag::HasDblData);
    flags.clear(TsStoreFlags::MetricFlag::HasDblStats);

    assert(!flags.is_set(TsStoreFlags::MetricFlag::HasIntData));
    assert(!flags.is_set(TsStoreFlags::MetricFlag::HasIntStats));
    assert(!flags.is_set(TsStoreFlags::MetricFlag::HasDblData));
    assert(!flags.is_set(TsStoreFlags::MetricFlag::HasDblStats));

    // Test severity
    flags.set_severity(TsStoreFlags::Severity::Critical);
    assert(flags.get_severity() == TsStoreFlags::Severity::Critical);

    flags.set_severity(TsStoreFlags::Severity::Debug);
    assert(flags.get_severity() == TsStoreFlags::Severity::Debug);

    flags.set_severity(TsStoreFlags::Severity::Error);
    assert(flags.get_severity() == TsStoreFlags::Severity::Error);

    flags.set_severity(TsStoreFlags::Severity::Fatal);
    assert(flags.get_severity() == TsStoreFlags::Severity::Fatal);

    flags.set_severity(TsStoreFlags::Severity::Info);
    assert(flags.get_severity() == TsStoreFlags::Severity::Info);

    flags.set_severity(TsStoreFlags::Severity::NotSet);
    assert(flags.get_severity() == TsStoreFlags::Severity::NotSet);

    flags.set_severity(TsStoreFlags::Severity::Trace);
    assert(flags.get_severity() == TsStoreFlags::Severity::Trace);

    flags.set_severity(TsStoreFlags::Severity::Warn);
    assert(flags.get_severity() == TsStoreFlags::Severity::Warn);

    flags.clear_severity();
    assert(flags.get_severity() == TsStoreFlags::Severity::NotSet);

    // Test to_string and serialization round-trip with all flag types set
    flags.set(TsStoreFlags::UserFlag::DatabaseEntry);
    flags.set(TsStoreFlags::UserFlag::HotCacheHint);
    flags.set(TsStoreFlags::UserFlag::IsExplicitNull);
    flags.set(TsStoreFlags::UserFlag::IsResult);
    flags.set(TsStoreFlags::UserFlag::KeeperRecord);
    flags.set(TsStoreFlags::UserFlag::LogConsole);
    flags.set(TsStoreFlags::UserFlag::SendNetwork);

    flags.set(TsStoreFlags::InternalFlag::HasData);
    flags.set(TsStoreFlags::InternalFlag::IsInvalid);

    flags.set(TsStoreFlags::MetricFlag::HasIntData);
    flags.set(TsStoreFlags::MetricFlag::HasIntStats);
    flags.set(TsStoreFlags::MetricFlag::HasDblData);
    flags.set(TsStoreFlags::MetricFlag::HasDblStats);

    flags.set_severity(TsStoreFlags::Severity::Critical);

    auto bytes = flags.to_bytes();
    TsStoreFlags flags2(bytes);
    assert(flags2.to_string() == flags.to_string());

    std::cout << "Flags 1: " << flags.to_string() << "\n";
    std::cout << "Flags 2: " << flags2.to_string() << "\n";

    std::cout << "\n\nAll TsStoreFlags tests PASSED!\n";
    return 0;
}