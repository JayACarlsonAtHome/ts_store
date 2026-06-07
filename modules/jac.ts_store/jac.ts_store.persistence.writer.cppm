module;

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <beman/ts_store/ts_store_headers/persistence/PersistCommon.hpp>
#include <beman/ts_store/ts_store_headers/persistence/EventSink.hpp>
#include <beman/ts_store/ts_store_headers/persistence/DoubleBufferedWriter.hpp>

export module jac.ts_store.persistence.writer;

export namespace jac::ts_store::inline_v001 {
    using jac::ts_store::inline_v001::PersistMode;
    using jac::ts_store::inline_v001::PersistedEvent;
    using jac::ts_store::inline_v001::IEventSink;
    using jac::ts_store::inline_v001::DoubleBufferedWriter;
}