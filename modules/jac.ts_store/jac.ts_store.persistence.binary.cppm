module;

#include <chrono>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <beman/ts_store/ts_store_headers/persistence/BinaryEventLog.hpp>
#include <beman/ts_store/ts_store_headers/persistence/BinaryEventSink.hpp>

export module jac.ts_store.persistence.binary;

export import jac.ts_store.persistence.common;

export namespace jac::ts_store::inline_v001 {
    using jac::ts_store::inline_v001::BinaryEventLogStats;
    using jac::ts_store::inline_v001::BinaryEventLog;
    using jac::ts_store::inline_v001::BinaryEventSink;
}