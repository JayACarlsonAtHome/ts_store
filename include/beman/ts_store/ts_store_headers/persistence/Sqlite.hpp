#pragma once

// Thin forwarder — canonical implementation lives in jacQLite (../jacQlite).
// Prefer: #include <jacQLite/Sqlite.hpp> or `import jac.qlite;`
#include <jacQLite/Sqlite.hpp>

namespace jac::ts_store {

using Sqlite = jac::qlite::Sqlite;
using SqliteError = jac::qlite::SqliteError;

} // namespace jac::ts_store