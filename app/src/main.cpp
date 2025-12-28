/**
 * SmartHub - Main Application Entry Point
 *
 * STM32MP157F-DK2 Smart Home Hub
 * Runs on Cortex-A7 under Buildroot Linux
 */

#include <smarthub/core/Application.hpp>
#include <smarthub/core/Logger.hpp>

#include <csignal>
#include <cstdlib>
#include <cstring>

using namespace smarthub;

// Global application pointer for signal handler
static Application* g_app = nullptr;

/**
 * Signal handler for graceful shutdown
 */
void signalHandler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        LOG_INFO("Received signal %d, initiating shutdown...", signum);
        if (g_app) {
            g_app->shutdown();
        }
    }
}

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
        g_app = &app;

        if (!app.initialize()) {
            LOG_ERROR("Failed to initialize application");
            exitCode = EXIT_FAILURE;
        } else {
            // Run main loop (blocks until shutdown)
            app.run();
        }

        g_app = nullptr;

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
