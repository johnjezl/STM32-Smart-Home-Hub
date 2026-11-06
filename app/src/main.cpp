/**
 * SmartHub - Main Application Entry Point
 *
 * STM32MP157F-DK2 Smart Home Hub
 * Runs on Cortex-A7 under Buildroot Linux
 */

#include <smarthub/core/Application.hpp>
#include <smarthub/core/Logger.hpp>

#include <unistd.h>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>

using namespace smarthub;

namespace {

// Written by the signal handler, so it has to be safe to use from one
std::atomic<bool> g_stopRequested{false};
static_assert(std::atomic<bool>::is_always_lock_free);

// A signal can arrive at any point, on any thread, so this can't take a lock, which rules out
// logging. Application::run notices the flag and shuts down on the main thread.
void signalHandler(int signum) {
    if (g_stopRequested.exchange(true)) {
        // A second signal means shutdown is stuck and someone wants out
        _exit(128 + signum);
    }
}

}  // namespace

/**
 * Print usage information
 */
void printUsage(const char* progName) {
    std::printf("SmartHub v%s - Smart Home Hub Application\n\n", Application::version());
    std::printf("Usage: %s [options]\n\n", progName);
    std::printf("Options:\n");
    std::printf("  -c, --config <path>  Path to configuration file\n");
    std::printf("                       (default: /etc/smarthub/config.yaml)\n");
    std::printf("  -v, --version        Print version and exit\n");
    std::printf("  -h, --help           Print this help message\n");
    std::printf("\n");
}

/**
 * Main entry point
 */
int main(int argc, char* argv[]) {
    // Default configuration path
    std::string configPath = "/etc/smarthub/config.yaml";

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-c") == 0 || std::strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) {
                configPath = argv[++i];
            } else {
                std::fprintf(stderr, "Error: -c/--config requires an argument\n");
                return EXIT_FAILURE;
            }
        } else if (std::strcmp(argv[i], "-v") == 0 || std::strcmp(argv[i], "--version") == 0) {
            std::printf("SmartHub v%s\n", Application::version());
            return EXIT_SUCCESS;
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return EXIT_SUCCESS;
        } else {
            std::fprintf(stderr, "Unknown option: %s\n", argv[i]);
            printUsage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    // Initialize logging early (will be reconfigured after config load)
    Logger::init(Logger::Level::Info, "");

    LOG_INFO("SmartHub v%s starting...", Application::version());

    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGPIPE, SIG_IGN); // Ignore broken pipe

    // Create and run application
    int exitCode = EXIT_SUCCESS;

    try {
        Application app(configPath);

        if (!app.initialize()) {
            LOG_ERROR("Failed to initialize application");
            exitCode = EXIT_FAILURE;
        } else {
            // Blocks until a signal asks to stop, then shuts down
            app.run(g_stopRequested);
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Fatal error: %s", e.what());
        exitCode = EXIT_FAILURE;
    } catch (...) {
        LOG_ERROR("Fatal error: unknown exception");
        exitCode = EXIT_FAILURE;
    }

    LOG_INFO("SmartHub shutdown complete");
    return exitCode;
}
