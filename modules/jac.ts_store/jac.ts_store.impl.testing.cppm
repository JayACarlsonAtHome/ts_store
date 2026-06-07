module;

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>
#include <sys/sysinfo.h>
#include <type_traits>

#include <beman/ts_store/ts_store_headers/impl_details/memory_guard.hpp>

export module jac.ts_store.impl.testing;

export import jac.ts_store.core;
export import jac.ts_store.test_options;

export namespace jac::ts_store::inline_v001 {
    using jac::ts_store::inline_v001::memory_guard;
}