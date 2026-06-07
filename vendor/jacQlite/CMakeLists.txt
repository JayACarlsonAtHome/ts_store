cmake_minimum_required(VERSION 3.20)
project(jacQLite VERSION 0.1.0 LANGUAGES CXX)

# jacQLite - Modern C++ SQLite wrapper with variadic "peel" patterns
# for clean, type-safe binding and querying.

option(JACQLITE_BUILD_EXAMPLES "Build example programs" ON)
option(JACQLITE_BUILD_TESTS "Build tests (uses in-tree jText-style framework)" OFF)
option(JACQLITE_WITH_JTEXT "Build jText integration tools and examples (requires sibling ../jText)" OFF)

add_library(jacQLite INTERFACE)
add_library(jacQLite::jacQLite ALIAS jacQLite)

target_include_directories(jacQLite INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Require modern C++ (C++20 for if constexpr, etc. used in the peel impl)
target_compile_features(jacQLite INTERFACE cxx_std_20)

# Find SQLite3 (required for linking, but we allow configuration without it for header inspection)
find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
    target_link_libraries(jacQLite INTERFACE SQLite::SQLite3)
    message(STATUS "SQLite3 found: ${SQLite3_VERSION}")
else()
    message(STATUS "SQLite3 not found. jacQLite headers can still be inspected, but linking will fail until SQLite3 is available.")
endif()

# Installation
include(GNUInstallDirs)
install(TARGETS jacQLite
    EXPORT jacQLiteTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT jacQLiteTargets
    FILE jacQLiteTargets.cmake
    NAMESPACE jacQLite::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jacQLite
)

# Examples
if(JACQLITE_BUILD_EXAMPLES)
    if(NOT SQLite3_FOUND)
        message(WARNING "SQLite3 not found - disabling examples build. Install sqlite-devel and reconfigure to build examples.")
        set(JACQLITE_BUILD_EXAMPLES OFF)
    else()
        add_subdirectory(examples)
    endif()
endif()

# Tests
if(JACQLITE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# jText integration (for round-tripping jText <-> SQLite)
if(JACQLITE_WITH_JTEXT)
    set(JTEXT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../jText")
    if(EXISTS "${JTEXT_SOURCE_DIR}/CMakeLists.txt")
        message(STATUS "jText integration enabled: using ${JTEXT_SOURCE_DIR}")
        # Build jText as subdirectory (it will produce jtext_core target)
        add_subdirectory(${JTEXT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/jText)
        add_subdirectory(tools/jtext_integration)
    else()
        message(WARNING "JACQLITE_WITH_JTEXT=ON but ../jText not found. Skipping jText integration.")
    endif()
endif()

message(STATUS "jacQLite configured.")
