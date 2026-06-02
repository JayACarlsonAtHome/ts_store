cmake_minimum_required(VERSION 3.26)
project(ts_store LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
target_include_directories(ts_store_flags PRIVATE ${TS_STORE_INCLUDE_DIR})
target_link_libraries(ts_store_flags PRIVATE project_warnings project_options)

set(TEST_NUMBERS 001 002 003 004 005 006 007)

foreach(num IN LISTS TEST_NUMBERS)
    # TS version
    add_executable(ts_store_${num}_TS tests/ts_store_${num}/test_${num}_TS.cpp)
    target_include_directories(ts_store_${num}_TS PRIVATE ${TS_STORE_INCLUDE_DIR})
    target_link_libraries(ts_store_${num}_TS PRIVATE project_warnings project_options)

    # XS version
    add_executable(ts_store_${num}_XS tests/ts_store_${num}/test_${num}_XS.cpp)
    target_include_directories(ts_store_${num}_XS PRIVATE ${TS_STORE_INCLUDE_DIR})
    target_link_libraries(ts_store_${num}_XS PRIVATE project_warnings project_options)
endforeach()

# =============================================================================
# Optional jText-based Persistence (high-throughput split file logging)
# =============================================================================
option(TS_STORE_ENABLE_JTEXT_PERSIST
       "Enable jText file persistence (requires sibling ../jText checkout)" OFF)

if(TS_STORE_ENABLE_JTEXT_PERSIST)
    message(STATUS "ts_store: jText persistence ENABLED")

    # We support building with either:
    #   - gcc-toolset-15 (GCC 15+)
    #   - Recent clang++ (Clang 16+ recommended)
    #
    # No compiler is forced. The user chooses via -DCMAKE_CXX_COMPILER=...
    # We only perform a basic capability check below.

    # Add jText as subdirectory (assumes sibling layout: ../jText)
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../jText/CMakeLists.txt)
        message(FATAL_ERROR "TS_STORE_ENABLE_JTEXT_PERSIST=ON but ../jText not found")
    endif()

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../jText ${CMAKE_BINARY_DIR}/jText_build)

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
            ${CMAKE_CURRENT_SOURCE_DIR}/../jText
    )

    target_link_libraries(ts_store_jtext_high_throughput_test
        PRIVATE
            project_warnings
            project_options
            jtext_core
    )

    target_include_directories(ts_store_jtext_high_throughput_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../jText
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
            ${CMAKE_CURRENT_SOURCE_DIR}/../jText
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
            ${CMAKE_CURRENT_SOURCE_DIR}/../jText
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
            ${CMAKE_CURRENT_SOURCE_DIR}/../jText
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
            ${CMAKE_CURRENT_SOURCE_DIR}/../jText
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
            ${CMAKE_CURRENT_SOURCE_DIR}/../jText
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
            ${CMAKE_CURRENT_SOURCE_DIR}/../jText
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

    # --- Support for stress tests 005/007 that now use JTextEventSink (for separate
    #     ints/floats metric files via JTextSplitEventLog + double buffering) ---
    foreach(num IN ITEMS 005 007)
        target_include_directories(ts_store_${num}_TS PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../jText)
        target_compile_definitions(ts_store_${num}_TS PRIVATE TS_STORE_ENABLE_JTEXT_PERSIST)
        target_link_libraries(ts_store_${num}_TS PRIVATE jtext_core)

        target_include_directories(ts_store_${num}_XS PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../jText)
        target_compile_definitions(ts_store_${num}_XS PRIVATE TS_STORE_ENABLE_JTEXT_PERSIST)
        target_link_libraries(ts_store_${num}_XS PRIVATE jtext_core)

        # Also compile the split log impl into these targets (like the jtext demos do)
        target_sources(ts_store_${num}_TS PRIVATE
            include/beman/ts_store/ts_store_headers/persistence/JTextSplitEventLog.cpp
        )
        target_sources(ts_store_${num}_XS PRIVATE
            include/beman/ts_store/ts_store_headers/persistence/JTextSplitEventLog.cpp
        )
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

target_include_directories(ts_store_binary_payload_benchmark
    PRIVATE
        ${TS_STORE_INCLUDE_DIR}
)

target_link_libraries(ts_store_binary_payload_benchmark
    PRIVATE
        project_warnings
        project_options
)