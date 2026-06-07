cmake_minimum_required(VERSION 3.26)
project(ts_store LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Werror -Wconversion
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
        -Wunused -Woverloaded-virtual -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion -Wformat=2
        -Wundef  -Wsuggest-override
        $<$<CXX_COMPILER_ID:GNU>:-Wno-interference-size>
)

add_library(project_options INTERFACE)
target_compile_options(project_options INTERFACE
          $<$<CONFIG:Release>:-O3>
          $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU>>:-march=native -mtune=native -fno-semantic-interposition>
          $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:Clang>>:-march=native -mtune=native -flto=thin>
)
option(ENABLE_CUSTOM_LLVM "Use custom LLVM/libc++ paths" OFF)

if(ENABLE_CUSTOM_LLVM)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(project_options INTERFACE
                $$   <   $$<CONFIG:Release>:-flto=thin>
        )
        target_link_options(project_options INTERFACE
                $$   <   $$<CONFIG:Release>:-flto=thin>
        )
        target_include_directories(project_options INTERFACE
                /home/jay/Tools/llvm-full_release_22_0/lib/clang/22/include
        )
        target_link_directories(project_options INTERFACE
                /home/jay/Tools/llvm-full_release_22_0/lib
        )
    endif()
endif()


# Fix include paths (was previously broken with ../ts_store_headers)
set(TS_STORE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Flags test
add_executable(ts_store_flags tests/ts_store_flags/Test_Flags.cpp)
target_compile_features(ts_store_flags PRIVATE cxx_std_23)
target_include_directories(ts_store_flags PRIVATE ${TS_STORE_INCLUDE_DIR})
target_link_libraries(ts_store_flags PRIVATE project_warnings project_options jac_ts_store_config jac_ts_store_ansi jac_ts_store_flags jac_ts_store_test_options jac_ts_store_persistence_binary)

# Reporting module: manifest + summarize + hub (incremental rebuild isolated from stress tests).
add_library(jac_report)
target_sources(jac_report
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES modules/jac.report/jac.report.cppm
    PRIVATE
        modules/jac.report/manifest.cpp
        modules/jac.report/summarize.cpp
)
target_compile_features(jac_report PUBLIC cxx_std_23)
target_include_directories(jac_report PUBLIC ${TS_STORE_INCLUDE_DIR})

# jText module shim (re-exports public jText API as jac.jtext).
add_library(jac_jtext)
target_sources(jac_jtext
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
            modules/jac.jtext/jac.jtext.core.cppm
            modules/jac.jtext/jac.jtext.reader.cppm
            modules/jac.jtext/jac.jtext.writer.cppm
            modules/jac.jtext/jac.jtext.cppm
)
target_compile_features(jac_jtext PUBLIC cxx_std_23)

# jacQLite module shim (header-only wrapper re-exported as jac.qlite).
add_library(jac_qlite)
target_sources(jac_qlite
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES modules/jac.qlite/jac.qlite.cppm
)
target_compile_features(jac_qlite PUBLIC cxx_std_23)

# ts_store config module (bounded_string, ts_store_config template).
add_library(jac_ts_store_config)
target_sources(jac_ts_store_config
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES modules/jac.ts_store/jac.ts_store.config.cppm
)
target_compile_features(jac_ts_store_config PUBLIC cxx_std_23)
target_include_directories(jac_ts_store_config PUBLIC ${TS_STORE_INCLUDE_DIR})

# ts_store ansi color helpers module.
add_library(jac_ts_store_ansi)
target_sources(jac_ts_store_ansi
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES modules/jac.ts_store/jac.ts_store.ansi.cppm
)
target_compile_features(jac_ts_store_ansi PUBLIC cxx_std_23)
target_include_directories(jac_ts_store_ansi PUBLIC ${TS_STORE_INCLUDE_DIR})

# ts_store flags module (TsStoreFlags + free-standing helpers).
add_library(jac_ts_store_flags)
target_sources(jac_ts_store_flags
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES modules/jac.ts_store/jac.ts_store.flags.cppm
)
target_compile_features(jac_ts_store_flags PUBLIC cxx_std_23)
target_include_directories(jac_ts_store_flags PUBLIC ${TS_STORE_INCLUDE_DIR})

# ts_store test CLI options (parse_test_options shim for module consumers).
add_library(jac_ts_store_test_options)
target_sources(jac_ts_store_test_options
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES modules/jac.ts_store/jac.ts_store.test_options.cppm
)
target_compile_features(jac_ts_store_test_options PUBLIC cxx_std_23)
target_include_directories(jac_ts_store_test_options PUBLIC ${TS_STORE_INCLUDE_DIR})

# ts_store shared persistence types (PersistMode, PersistedEvent, IEventSink).
add_library(jac_ts_store_persistence_common)
target_sources(jac_ts_store_persistence_common
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES modules/jac.ts_store/jac.ts_store.persistence.common.cppm
)
target_compile_features(jac_ts_store_persistence_common PUBLIC cxx_std_23)
target_include_directories(jac_ts_store_persistence_common PUBLIC ${TS_STORE_INCLUDE_DIR})

# ts_store binary persistence module (BinaryEventLog + BinaryEventSink; header-only impl).
add_library(jac_ts_store_persistence_binary)
target_sources(jac_ts_store_persistence_binary
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES modules/jac.ts_store/jac.ts_store.persistence.binary.cppm
)
target_compile_features(jac_ts_store_persistence_binary PUBLIC cxx_std_23)
target_include_directories(jac_ts_store_persistence_binary PUBLIC ${TS_STORE_INCLUDE_DIR})
target_link_libraries(jac_ts_store_persistence_binary PUBLIC jac_ts_store_persistence_common)

# ts_store double-buffered writer module (header-only DoubleBufferedWriter).
add_library(jac_ts_store_persistence_writer)
target_sources(jac_ts_store_persistence_writer
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES modules/jac.ts_store/jac.ts_store.persistence.writer.cppm
)
target_compile_features(jac_ts_store_persistence_writer PUBLIC cxx_std_23)
target_include_directories(jac_ts_store_persistence_writer PUBLIC ${TS_STORE_INCLUDE_DIR})
target_link_libraries(jac_ts_store_persistence_writer PUBLIC jac_ts_store_persistence_common)

# ts_store core module (ts_store<Config> + impl_details; header-only).
add_library(jac_ts_store_core)
target_sources(jac_ts_store_core
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES modules/jac.ts_store/jac.ts_store.core.cppm
)
target_compile_features(jac_ts_store_core PUBLIC cxx_std_23)
target_include_directories(jac_ts_store_core PUBLIC ${TS_STORE_INCLUDE_DIR})
target_link_libraries(jac_ts_store_core PUBLIC
    jac_ts_store_config
    jac_ts_store_flags
    jac_ts_store_ansi
    jac_ts_store_persistence_writer
)

# ts_store stress-test umbrella (core + CLI options + memory_guard).
add_library(jac_ts_store_impl_testing)
target_sources(jac_ts_store_impl_testing
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES modules/jac.ts_store/jac.ts_store.impl.testing.cppm
)
target_compile_features(jac_ts_store_impl_testing PUBLIC cxx_std_23)
target_include_directories(jac_ts_store_impl_testing PUBLIC ${TS_STORE_INCLUDE_DIR})
target_link_libraries(jac_ts_store_impl_testing PUBLIC jac_ts_store_core jac_ts_store_test_options)

# Test matrix runner module (params, scenarios, execution). Re-exports jac.report.
add_library(jac_test_framework)
target_sources(jac_test_framework
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES modules/jac.test_framework/jac.test_framework.cppm
    PRIVATE
        modules/jac.test_framework/runner.cpp
)
target_compile_features(jac_test_framework PUBLIC cxx_std_23)
target_link_libraries(jac_test_framework PUBLIC jac_report)

# C++ Test CLI Tool - the proper CLI for running the full matrix.
# Replaces the old shell and Python runners. Built as a first-class tool.
# Run it from a directory that contains the built ts_store_* test binaries,
# or extend it with --bin-dir support in the future.
add_executable(ts_test_cli tools/test_cli/main.cpp)
target_compile_features(ts_test_cli PRIVATE cxx_std_23)
target_include_directories(ts_test_cli PRIVATE ${TS_STORE_INCLUDE_DIR})
target_link_libraries(ts_test_cli PRIVATE jac_test_framework)

set(TEST_NUMBERS 001 002 003 004 005 006 007)

foreach(num IN LISTS TEST_NUMBERS)
    # TS version
    add_executable(ts_store_${num}_TS tests/ts_store_${num}/test_${num}_TS.cpp)
    target_include_directories(ts_store_${num}_TS PRIVATE ${TS_STORE_INCLUDE_DIR})
    target_link_libraries(ts_store_${num}_TS PRIVATE project_warnings project_options jac_ts_store_impl_testing jac_ts_store_persistence_common jac_ts_store_persistence_binary)

    # XS version
    add_executable(ts_store_${num}_XS tests/ts_store_${num}/test_${num}_XS.cpp)
    target_include_directories(ts_store_${num}_XS PRIVATE ${TS_STORE_INCLUDE_DIR})
    target_link_libraries(ts_store_${num}_XS PRIVATE project_warnings project_options jac_ts_store_impl_testing jac_ts_store_persistence_common jac_ts_store_persistence_binary)
endforeach()

# =============================================================================
# Optional jText-based Persistence (high-throughput split file logging)
# =============================================================================
option(TS_STORE_ENABLE_JTEXT_PERSIST
       "Enable jText file persistence" OFF)

option(TS_STORE_ENABLE_SQLITE_PERSIST
       "Enable direct SQLite persistence (requires libsqlite3)" OFF)

set(TS_STORE_JACQLITE_MODE "reference" CACHE STRING
    "jacQLite source mode: 'reference' (sibling ../jacQlite) or 'vendored' (vendor/jacQlite)")

set(TS_STORE_JTEXT_MODE "reference" CACHE STRING
    "jText source mode: 'reference' (use sibling ../jText) or 'vendored' (use local vendor/jText)")

if(TS_STORE_ENABLE_JTEXT_PERSIST)
    message(STATUS "ts_store: jText persistence ENABLED")

    # Tell the jText subdir (whether reference or vendored) not to define the CLI tool executables
    # (we define our own copies below from the appropriate location).
    set(TS_STORE_BUILDING_JTEXT_TOOLS ON)

    # We support building with either:
    #   - gcc-toolset-15 (GCC 15+)
    #   - Recent clang++ (Clang 16+ recommended)
    #
    # No compiler is forced. The user chooses via -DCMAKE_CXX_COMPILER=...
    # We only perform a basic capability check below.

    if(TS_STORE_JTEXT_MODE STREQUAL "vendored")
        set(JTEXT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/jText")
        set(JTEXT_CLI_DIR "${JTEXT_SOURCE_DIR}/CLI")
        if(NOT EXISTS "${JTEXT_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR
                "TS_STORE_JTEXT_MODE=vendored but ${JTEXT_SOURCE_DIR} not found.\n"
                "Run: ./scripts/Sync_dependencies.sh --sync jText")
        endif()
        add_subdirectory(${JTEXT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/jText_build)
    else()
        # reference mode (default for development with live sibling)
        set(JTEXT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../jText")
        set(JTEXT_CLI_DIR "${JTEXT_SOURCE_DIR}/src/tool")
        if(NOT EXISTS "${JTEXT_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR "TS_STORE_ENABLE_JTEXT_PERSIST=ON but ../jText not found")
        endif()
        add_subdirectory(${JTEXT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/jText_build)
    endif()

    # ts_store jText persistence module (JTextSplitEventLog + JTextEventSink).
    add_library(jac_ts_store_persistence_jtext)
    target_sources(jac_ts_store_persistence_jtext
        PUBLIC
            FILE_SET cxx_modules TYPE CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
            FILES modules/jac.ts_store/jac.ts_store.persistence.jtext.cppm
        PRIVATE
            include/beman/ts_store/ts_store_headers/persistence/JTextSplitEventLog.cpp
    )
    target_compile_features(jac_ts_store_persistence_jtext PUBLIC cxx_std_23)
    target_include_directories(jac_ts_store_persistence_jtext
        PUBLIC ${TS_STORE_INCLUDE_DIR}
        PRIVATE ${JTEXT_SOURCE_DIR}
    )
    target_link_libraries(jac_ts_store_persistence_jtext PUBLIC jac_jtext jac_ts_store_persistence_common PRIVATE jtext_core)

    # Tell the jText subdir not to define the CLI tool executables (we define our own copies below)
    set(TS_STORE_BUILDING_JTEXT_TOOLS ON)

    # jText CLI tools.
    # - In reference mode: use the local maintained copies in tools/jtext_cli/ (original behavior)
    # - In vendored mode: use the synced copies from vendor/jText/CLI/
    if(TS_STORE_JTEXT_MODE STREQUAL "vendored")
        set(_jtext_cli_process_src "${JTEXT_CLI_DIR}/jtext_process.cpp")
        set(_jtext_cli_retrieve_src "${JTEXT_CLI_DIR}/jtext_retrieve.cpp")
    else()
        set(_jtext_cli_process_src "tools/jtext_cli/jtext_process.cpp")
        set(_jtext_cli_retrieve_src "tools/jtext_cli/jtext_retrieve.cpp")
    endif()

    add_executable(jtext_process
        ${_jtext_cli_process_src}
    )
    target_link_libraries(jtext_process PRIVATE jtext_core)
    target_include_directories(jtext_process PRIVATE
        ${JTEXT_SOURCE_DIR}/src
        ${JTEXT_SOURCE_DIR}
    )
    target_compile_features(jtext_process PRIVATE cxx_std_23)

    add_executable(jtext_retrieve
        ${_jtext_cli_retrieve_src}
    )
    target_include_directories(jtext_retrieve PRIVATE
        ${JTEXT_SOURCE_DIR}/src
        ${JTEXT_SOURCE_DIR}
    )
    target_compile_features(jtext_retrieve PRIVATE cxx_std_23)

    # --- Compiler capability check (dual compiler support) ---
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "13")
            message(FATAL_ERROR
                "TS_STORE_ENABLE_JTEXT_PERSIST requires GCC >= 13 or Clang >= 16.\n"
                "Current: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\n"
                "Tip: Use 'scl enable gcc-toolset-15 -- cmake ...' or pass -DCMAKE_CXX_COMPILER=clang++")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16")
            message(WARNING
                "Clang ${CMAKE_CXX_COMPILER_VERSION} may have incomplete C++23 support. Clang 16+ recommended.")
        endif()
    endif()

    # Persistence demo target
    add_executable(ts_store_jtext_high_throughput_test
        examples/jtext_high_throughput_test.cpp
    )

    target_include_directories(ts_store_jtext_high_throughput_test
        PRIVATE
            ${JTEXT_SOURCE_DIR}
    )

    target_link_libraries(ts_store_jtext_high_throughput_test
        PRIVATE
            project_warnings
            project_options
            jtext_core
    )

    target_include_directories(ts_store_jtext_high_throughput_test
        PRIVATE
            ${JTEXT_SOURCE_DIR}
    )

    target_compile_definitions(ts_store_jtext_high_throughput_test
        PRIVATE
            TS_STORE_ENABLE_JTEXT_PERSIST
    )

    target_link_libraries(ts_store_jtext_high_throughput_test
        PRIVATE
            project_warnings
            project_options
            jtext_core
    )

    # jText split demo
    add_executable(ts_store_jtext_split_demo
        examples/jtext_split_persistence_demo.cpp
        include/beman/ts_store/ts_store_headers/persistence/JTextSplitEventLog.cpp
    )

    # Dedicated jText high-throughput test (for fair comparison with binary)
    add_executable(ts_store_jtext_throughput_test
        examples/jtext_throughput_test.cpp
        include/beman/ts_store/ts_store_headers/persistence/JTextSplitEventLog.cpp
    )

    target_include_directories(ts_store_jtext_throughput_test
        PRIVATE
            ${TS_STORE_INCLUDE_DIR}
            ${JTEXT_SOURCE_DIR}
    )

    target_link_libraries(ts_store_jtext_throughput_test
        PRIVATE
            project_warnings
            project_options
            jtext_core
    )

    target_include_directories(ts_store_jtext_split_demo
        PRIVATE
            ${TS_STORE_INCLUDE_DIR}
            ${JTEXT_SOURCE_DIR}
    )

    target_link_libraries(ts_store_jtext_split_demo
        PRIVATE
            project_warnings
            project_options
            jtext_core
    )

    # Binary fast persistence demo / comparison tool
    add_executable(ts_store_binary_persist_demo
        examples/binary_persist_demo.cpp
        include/beman/ts_store/ts_store_headers/persistence/JTextSplitEventLog.cpp
    )

    target_include_directories(ts_store_binary_persist_demo
        PRIVATE
            ${TS_STORE_INCLUDE_DIR}
            ${JTEXT_SOURCE_DIR}
    )

    target_link_libraries(ts_store_binary_persist_demo
        PRIVATE
            project_warnings
            project_options
            jtext_core
    )

    # Payload scaling benchmarks (user-requested matrix)
    add_executable(ts_store_jtext_payload_benchmark
        examples/jtext_payload_benchmark.cpp
        include/beman/ts_store/ts_store_headers/persistence/JTextSplitEventLog.cpp
    )

    target_include_directories(ts_store_jtext_payload_benchmark
        PRIVATE
            ${TS_STORE_INCLUDE_DIR}
            ${JTEXT_SOURCE_DIR}
    )

    target_link_libraries(ts_store_jtext_payload_benchmark
        PRIVATE
            project_warnings
            project_options
            jtext_core
    )

    # Double-buffered pluggable persistence demo (jText + Binary sinks)
    add_executable(ts_store_double_buffer_demo
        examples/double_buffered_persistence_demo.cpp
        include/beman/ts_store/ts_store_headers/persistence/JTextSplitEventLog.cpp
    )

    target_include_directories(ts_store_double_buffer_demo
        PRIVATE
            ${TS_STORE_INCLUDE_DIR}
            ${JTEXT_SOURCE_DIR}
    )

    target_compile_definitions(ts_store_double_buffer_demo
        PRIVATE
            TS_STORE_ENABLE_JTEXT_PERSIST
    )

    target_link_libraries(ts_store_double_buffer_demo
        PRIVATE
            project_warnings
            project_options
            jtext_core
    )

    # Integrated demo: ts_store + attach_persistence (double buffered)
    add_executable(ts_store_integrated_double_buffer_demo
        examples/ts_store_with_double_buffer_demo.cpp
        include/beman/ts_store/ts_store_headers/persistence/JTextSplitEventLog.cpp
        include/beman/ts_store/ts_store_headers/persistence/BinaryEventLogReader.cpp
    )

    target_include_directories(ts_store_integrated_double_buffer_demo
        PRIVATE
            ${TS_STORE_INCLUDE_DIR}
            ${JTEXT_SOURCE_DIR}
    )

    target_compile_definitions(ts_store_integrated_double_buffer_demo
        PRIVATE
            TS_STORE_ENABLE_JTEXT_PERSIST
    )

    target_link_libraries(ts_store_integrated_double_buffer_demo
        PRIVATE
            project_warnings
            project_options
            jtext_core
    )

    # --- Support for *all* stress tests (001-007) to use double-buffered persistence
    #     with either JTextEventSink or BinaryEventSink (chosen at runtime via --persist).
    #     This enables the new test_results/ binary_logs / jText_logs output for every test.
    foreach(num IN LISTS TEST_NUMBERS)
        target_include_directories(ts_store_${num}_TS PRIVATE ${JTEXT_SOURCE_DIR})
        target_compile_definitions(ts_store_${num}_TS PRIVATE TS_STORE_ENABLE_JTEXT_PERSIST)
        target_link_libraries(ts_store_${num}_TS PRIVATE jtext_core jac_ts_store_persistence_jtext)

        target_include_directories(ts_store_${num}_XS PRIVATE ${JTEXT_SOURCE_DIR})
        target_compile_definitions(ts_store_${num}_XS PRIVATE TS_STORE_ENABLE_JTEXT_PERSIST)
        target_link_libraries(ts_store_${num}_XS PRIVATE jtext_core jac_ts_store_persistence_jtext)
    endforeach()
endif()

# =============================================================================
# Optional direct SQLite persistence
# =============================================================================
if(TS_STORE_ENABLE_SQLITE_PERSIST)
    message(STATUS "ts_store: SQLite persistence ENABLED")

    if(TS_STORE_JACQLITE_MODE STREQUAL "vendored")
        set(JACQLITE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/jacQlite")
        if(NOT EXISTS "${JACQLITE_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR
                "TS_STORE_JACQLITE_MODE=vendored but ${JACQLITE_SOURCE_DIR} not found.")
        endif()
    else()
        set(JACQLITE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../jacQlite")
        if(NOT EXISTS "${JACQLITE_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR "TS_STORE_ENABLE_SQLITE_PERSIST=ON but ../jacQlite not found")
        endif()
    endif()

    set(JACQLITE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(JACQLITE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(JACQLITE_WITH_JTEXT OFF CACHE BOOL "" FORCE)
    add_subdirectory(${JACQLITE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/jacQlite_build)

    target_link_libraries(jac_qlite PUBLIC jacQLite::jacQLite)

    # ts_store SQL persistence module (SqlEventSink).
    add_library(jac_ts_store_persistence_sql)
    target_sources(jac_ts_store_persistence_sql
        PUBLIC
            FILE_SET cxx_modules TYPE CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
            FILES modules/jac.ts_store/jac.ts_store.persistence.sql.cppm
        PRIVATE
            include/beman/ts_store/ts_store_headers/persistence/SqlEventSink.cpp
    )
    target_compile_features(jac_ts_store_persistence_sql PUBLIC cxx_std_23)
    target_include_directories(jac_ts_store_persistence_sql PUBLIC ${TS_STORE_INCLUDE_DIR})
    target_link_libraries(jac_ts_store_persistence_sql PUBLIC jac_qlite jac_ts_store_persistence_common PRIVATE jacQLite::jacQLite)

    foreach(num IN LISTS TEST_NUMBERS)
        target_compile_definitions(ts_store_${num}_TS PRIVATE TS_STORE_ENABLE_SQLITE_PERSIST)
        target_link_libraries(ts_store_${num}_TS PRIVATE jac_ts_store_persistence_sql)

        target_compile_definitions(ts_store_${num}_XS PRIVATE TS_STORE_ENABLE_SQLITE_PERSIST)
        target_link_libraries(ts_store_${num}_XS PRIVATE jac_ts_store_persistence_sql)
    endforeach()
endif()

# -----------------------------------------------------------------------------
# Binary-only persistence demos (no jText dependency; always available)
# These demonstrate / benchmark the fast mmap binary path independently.
# -----------------------------------------------------------------------------
# Dedicated binary high-throughput test
add_executable(ts_store_binary_throughput_test
    examples/binary_throughput_test.cpp
)

target_include_directories(ts_store_binary_throughput_test
    PRIVATE
        ${TS_STORE_INCLUDE_DIR}
)

target_link_libraries(ts_store_binary_throughput_test
    PRIVATE
        project_warnings
        project_options
)

# Payload scaling benchmarks (user-requested matrix)
add_executable(ts_store_binary_payload_benchmark
    examples/binary_payload_benchmark.cpp
)

# Pure in-memory hot path benchmark (no persistence attached)
add_executable(ts_store_in_memory_throughput
    examples/in_memory_throughput.cpp
)

target_include_directories(ts_store_in_memory_throughput
    PRIVATE
        ${TS_STORE_INCLUDE_DIR}
)

target_link_libraries(ts_store_in_memory_throughput
    PRIVATE
        project_warnings
        project_options
)

if(TS_STORE_ENABLE_SQLITE_PERSIST)
    # Example slurper: takes jText split files + inserts into SQLite
    add_executable(ts_store_slurp_jtext_to_sqlite
        examples/slurp_jtext_to_sqlite.cpp
    )
    target_include_directories(ts_store_slurp_jtext_to_sqlite PRIVATE
        ${TS_STORE_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../jText
    )
    target_link_libraries(ts_store_slurp_jtext_to_sqlite PRIVATE
        project_warnings
        project_options
        jacQLite::jacQLite
        jtext_core
    )
endif()

target_include_directories(ts_store_binary_payload_benchmark
    PRIVATE
        ${TS_STORE_INCLUDE_DIR}
)

target_link_libraries(ts_store_binary_payload_benchmark
    PRIVATE
        project_warnings
        project_options
)

# jac.report + ts_test_cli (jText write, SQLite views)
if(TS_STORE_ENABLE_JTEXT_PERSIST)
    target_include_directories(jac_jtext PUBLIC ${JTEXT_SOURCE_DIR} ${JTEXT_SOURCE_DIR}/src)
    target_link_libraries(jac_jtext PUBLIC jtext_core)
    target_link_libraries(jac_report PUBLIC jac_jtext)
    target_link_libraries(ts_test_cli PRIVATE project_warnings project_options)
else()
    message(WARNING "jac.report run_manifest requires TS_STORE_ENABLE_JTEXT_PERSIST=ON")
endif()

if(TS_STORE_ENABLE_SQLITE_PERSIST)
    target_link_libraries(jac_report PUBLIC jac_qlite)
else()
    message(WARNING "jac.report summarize requires TS_STORE_ENABLE_SQLITE_PERSIST=ON")
endif()