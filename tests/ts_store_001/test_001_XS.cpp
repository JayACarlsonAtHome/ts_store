//tests/ts_store_001/Test_001_XS.CPP

#include "../../include/beman/ts_store/ts_store_headers/ts_store.hpp"
#include "../../include/beman/ts_store/ts_store_headers/persistence/DoubleBufferedWriter.hpp"
#include "../../include/beman/ts_store/ts_store_headers/persistence/BinaryEventSink.hpp"
#include "../../include/beman/ts_store/ts_store_headers/persistence/JTextEventSink.hpp"
#include "../../include/beman/ts_store/ts_store_headers/persistence/PersistCommon.hpp"
#include "../../include/beman/ts_store/ts_store_headers/persistence/EventSink.hpp"

using namespace jac::ts_store::inline_v001;

using LogConfig = ts_store_config<false, 6, 20, 43, 9, 6, false, false, false, false>;
using LogxStore = ts_store<LogConfig>;

int main(int argc, char** argv) {
    auto _opts = jac::ts_store::inline_v001::parse_test_options(argc, argv);
    (void)_opts; // silence -Wunused (CLI sets envs; we also read persist/base below)
    if (std::cin.rdbuf()->in_avail() > 0) {
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    constexpr size_t threads = 8;
    constexpr size_t events  = 8;
    std::cout << ansi::blue() << "=== ts_store — Simple Test 001 XS "
                               "-- Equivalent to MultiThread Hello World  ===" << ansi::reset() << "\n\n";

    LogxStore prod(threads, events);

    // Attach double-buffered (asynchronous) persistence for this test run.
    // Chosen via --persist binary|jtext (default jtext). --base-name can override the
    // output file prefix (runner uses this to place files under test_results/*/TS_STORE_TEST_.../).
    {
        std::string ptype = _opts.persist.empty() ? "jtext" : _opts.persist;
        std::string bname = _opts.base_name;
        if (bname.empty()) bname = "persist";

        std::unique_ptr<IEventSink> sink;
        const size_t im = LogConfig::the_IntMetrics;
        const size_t dm = LogConfig::the_DblMetrics;
        if (ptype == "binary") {
            sink = std::make_unique<BinaryEventSink>(bname, im, dm, PersistMode::All);
        } else {
            sink = std::make_unique<JTextEventSink>(bname, im, dm, PersistMode::All);
        }
        auto writer = std::make_unique<DoubleBufferedWriter>(std::move(sink), 10'000);
        prod.attach_persistence(std::move(writer));
    }

    prod.test_run(); //Test Run sets Debug == True
    if (!prod.verify_level01()) {
        std::cerr << "PRODUCTION SIMULATION FAILED — structural corruption\n";
        prod.diagnose_failures();
        return 1;
    }
    if (!prod.verify_level02()) {
        std::cerr << "PRODUCTION SIMULATION FAILED — test payload corruption\n";
        prod.diagnose_failures();
        return 1;
    }
    std::cout << "PRODUCTION SIMULATION PASSED — 100% clean\n";
    prod.press_any_key();
    prod.print(0);
    std::cout << "\n=== ALL TESTS COMPLETED SUCCESSFULLY ===\n";
    return 0;
}
