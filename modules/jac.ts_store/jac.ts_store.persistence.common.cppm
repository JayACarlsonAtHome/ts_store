module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <beman/ts_store/ts_store_headers/persistence/PersistCommon.hpp>
#include <beman/ts_store/ts_store_headers/persistence/EventSink.hpp>
#include <beman/ts_store/ts_store_headers/persistence/FlagRoutingEventSink.hpp>

export module jac.ts_store.persistence.common;

export namespace jac::ts_store::inline_v001 {
    using jac::ts_store::inline_v001::PersistMode;
    using jac::ts_store::inline_v001::PersistedEvent;
    using jac::ts_store::inline_v001::IEventSink;
    using jac::ts_store::inline_v001::FlagRoutingEventSink;
}