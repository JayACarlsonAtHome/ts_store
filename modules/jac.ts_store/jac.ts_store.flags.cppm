module;

#include <array>
#include <bitset>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <beman/ts_store/ts_store_headers/ts_store_flags.hpp>

export module jac.ts_store.flags;

export using ::TsStoreFlags;
export using ::flags_set_has_data;
export using ::flags_clear_has_data;
export using ::set_user_flag;
export using ::set_internal_flag;
export using ::set_metric_flag;
export using ::set_severity;