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
    bool interactive = true;
    bool color = true;
};

inline TestOptions parse_test_options(int argc, char** argv) {
    TestOptions opts;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strcmp(arg, "--no-interactive") == 0 ||
            std::strcmp(arg, "--interactive=0") == 0 ||
            std::strcmp(arg, "--interactive=false") == 0) {
            opts.interactive = false;
        } else if (std::strcmp(arg, "--interactive") == 0 ||
                   std::strcmp(arg, "--interactive=1") == 0 ||
                   std::strcmp(arg, "--interactive=true") == 0) {
            opts.interactive = true;
        } else if (std::strcmp(arg, "--no-color") == 0 ||
                   std::strcmp(arg, "--color=0") == 0 ||
                   std::strcmp(arg, "--color=false") == 0) {
            opts.color = false;
        } else if (std::strcmp(arg, "--color") == 0 ||
                   std::strcmp(arg, "--color=1") == 0 ||
                   std::strcmp(arg, "--color=true") == 0) {
            opts.color = true;
        } else if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
            // optional: could print, but for now silent
        }
    }

    // Set the env vars so that the existing is_interactive() / colors_enabled()
    // (which check getenv) pick up the CLI choice. This keeps the control
    // logic in one place and makes CLI set "the values the programs use".
    if (!opts.interactive) {
        setenv("TS_STORE_INTERACTIVE", "0", 1);
    } else if (opts.interactive) {
        // only set if explicitly 1? but default is on, so if flag for 1 set it
        // actually, to force, but since default true, if not set to 0, leave or set 1
        setenv("TS_STORE_INTERACTIVE", "1", 1);
    }

    if (!opts.color) {
        setenv("TS_STORE_COLOR", "0", 1);
    } else if (opts.color) {
        setenv("TS_STORE_COLOR", "1", 1);
    }

    return opts;
}

} // namespace
