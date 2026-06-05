// ts_store/ts_store_headers/impl_details/test_options.hpp
// CLI options for controlling interactive pauses and color output in test/example programs.
// These are runtime flags (passed on command line to the binaries).
// The runner uses them for automation to avoid human input waits and to keep logs clean.

#pragma once

#include <string>
#include <cstring>
#include <cstdlib>

namespace jac::ts_store::inline_v001 {

struct TestOptions {
    bool interactive = false;
    bool color = false;
    std::string persist = "jtext";   // "jtext", "binary", "sql" (direct), or "none" (pure in-memory) for persistence sink choice
    std::string base_name;           // base name (can include path) for the persist log files
    // Test size parameters (for SSD-friendly smoke tests vs full intensity)
    // Defaults are high-intensity numbers. Runner/config can override with --threads etc or --test-size=smoke
    size_t threads = 250;
    size_t events_per_thread = 4000;
    size_t runs = 50;
    std::string test_size;  // "smoke" | "full" (optional profile selector)
};

inline TestOptions parse_test_options(int argc, char** argv) {
    TestOptions opts;  // defaults: no interactive, no colors (as required for tests by default)
    bool interactive_set = false;
    bool color_set = false;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strcmp(arg, "--no-interactive") == 0 ||
            std::strcmp(arg, "--interactive=0") == 0 ||
            std::strcmp(arg, "--interactive=false") == 0) {
            opts.interactive = false;
            interactive_set = true;
        } else if (std::strcmp(arg, "--interactive") == 0 ||
                   std::strcmp(arg, "--interactive=1") == 0 ||
                   std::strcmp(arg, "--interactive=true") == 0) {
            opts.interactive = true;
            interactive_set = true;
        } else if (std::strcmp(arg, "--no-color") == 0 ||
                   std::strcmp(arg, "--color=0") == 0 ||
                   std::strcmp(arg, "--color=false") == 0) {
            opts.color = false;
            color_set = true;
        } else if (std::strcmp(arg, "--color") == 0 ||
                   std::strcmp(arg, "--color=1") == 0 ||
                   std::strcmp(arg, "--color=true") == 0) {
            opts.color = true;
            color_set = true;
        } else if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
            // optional: could print, but for now silent
        } else if (std::strncmp(arg, "--persist=", 10) == 0) {
            opts.persist = (arg + 10);
        } else if (std::strcmp(arg, "--persist") == 0 && (i + 1) < argc) {
            opts.persist = argv[++i];
        } else if (std::strncmp(arg, "--base-name=", 12) == 0) {
            opts.base_name = (arg + 12);
        } else if (std::strcmp(arg, "--base-name") == 0 && (i + 1) < argc) {
            opts.base_name = argv[++i];
        } else if (std::strncmp(arg, "--threads=", 10) == 0) {
            opts.threads = std::stoull(arg + 10);
        } else if (std::strcmp(arg, "--threads") == 0 && (i + 1) < argc) {
            opts.threads = std::stoull(argv[++i]);
        } else if (std::strncmp(arg, "--events-per-thread=", 20) == 0) {
            opts.events_per_thread = std::stoull(arg + 20);
        } else if (std::strcmp(arg, "--events-per-thread") == 0 && (i + 1) < argc) {
            opts.events_per_thread = std::stoull(argv[++i]);
        } else if (std::strncmp(arg, "--runs=", 7) == 0) {
            opts.runs = std::stoull(arg + 7);
        } else if (std::strcmp(arg, "--runs") == 0 && (i + 1) < argc) {
            opts.runs = std::stoull(argv[++i]);
        } else if (std::strncmp(arg, "--test-size=", 12) == 0) {
            opts.test_size = (arg + 12);
        } else if (std::strcmp(arg, "--test-size") == 0 && (i + 1) < argc) {
            opts.test_size = argv[++i];
        }
    }

    // Only set env if CLI explicitly provided the option. This way:
    // - no params: no env set, helpers fall back to Config defaults (now false) + isatty logic
    // - explicit CLI flag: force the env so helpers use it (CLI wins over everything)
    if (interactive_set) {
        setenv("TS_STORE_INTERACTIVE", opts.interactive ? "1" : "0", 1);
    }
    if (color_set) {
        setenv("TS_STORE_COLOR", opts.color ? "1" : "0", 1);
    }

    // Apply test-size profile if specified (smoke for quick/SSD-safe ~100 records, full for high intensity)
    if (opts.test_size == "smoke") {
        opts.threads = 5;
        opts.events_per_thread = 20;
        opts.runs = 1;
    } else if (opts.test_size == "full") {
        opts.threads = 250;
        opts.events_per_thread = 4000;
        opts.runs = 50;
    }

    return opts;
}

} // namespace
