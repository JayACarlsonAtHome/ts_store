module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <beman/ts_store/ts_store_headers/persistence/JTextSplitEventLog.hpp>
#include <beman/ts_store/ts_store_headers/persistence/JTextEventSink.hpp>

export module jac.ts_store.persistence.jtext;

export import jac.ts_store.persistence.common;

export namespace jac::ts_store::inline_v001 {
    using jac::ts_store::inline_v001::JTextSplitEventLogStats;
    using jac::ts_store::inline_v001::JTextSplitEventLog;
    using jac::ts_store::inline_v001::JTextEventSink;
}