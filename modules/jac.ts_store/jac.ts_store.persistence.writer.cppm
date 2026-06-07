module;

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include <beman/ts_store/ts_store_headers/persistence/DoubleBufferedWriter.hpp>

export module jac.ts_store.persistence.writer;

export import jac.ts_store.persistence.common;

export namespace jac::ts_store::inline_v001 {
    using jac::ts_store::inline_v001::DoubleBufferedWriter;
}