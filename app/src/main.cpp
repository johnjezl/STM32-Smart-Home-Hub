/**
 * SmartHub - Main Application Entry Point
 *
 * STM32MP157F-DK2 Smart Home Hub
 * Runs on Cortex-A7 under Buildroot Linux
 */

#include <smarthub/core/Application.hpp>
#include <smarthub/core/Config.hpp>
#include <smarthub/core/Logger.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/security/UserManager.hpp>

#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>

using namespace smarthub;

namespace {

// Written by the signal handler, so these have to be safe to use from one
std::atomic<bool> g_stopRequested{false};
std::atomic<int64_t> g_firstSignalSeconds{-1};
std::atomic<int> g_signalNumber{0};
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<int64_t>::is_always_lock_free);
static_assert(std::atomic<int>::is_always_lock_free);

// Long enough that a repeated Ctrl-C doesn't cut off a shutdown that's going fine
constexpr int64_t kForceExitAfterSeconds = 5;

// A signal can arrive at any point, on any thread, so this can't take a lock, which rules out
// logging. Application::run notices the flag and shuts down on the main thread.
void signalHandler(int signum) {
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);

    // Signals can overlap, so exactly one of them records the time, before anything can read it
    int64_t first = -1;
    if (g_firstSignalSeconds.compare_exchange_strong(first, now.tv_sec)) {
        g_signalNumber = signum;
    } else if (now.tv_sec - first >= kForceExitAfterSeconds) {
        // Another signal this much later means shutdown is stuck and someone wants out
        _exit(128 + signum);
    }
    g_stopRequested = true;
}

// Reads a line from stdin, without echoing it if that's a terminal
bool readSecretLine(const char* prompt, std::string& line) {
    const bool terminal = isatty(STDIN_FILENO) != 0;
    termios saved{};
    bool restore = false;
    if (terminal) {
        std::fprintf(stderr, "%s", prompt);
        std::fflush(stderr);
        if (tcgetattr(STDIN_FILENO, &saved) == 0) {
            termios quiet = saved;
            quiet.c_lflag &= ~static_cast<tcflag_t>(ECHO);
            restore = tcsetattr(STDIN_FILENO, TCSANOW, &quiet) == 0;
        }
    }
    const bool read = static_cast<bool>(std::getline(std::cin, line));
    if (restore) {
        tcsetattr(STDIN_FILENO, TCSANOW, &saved);
    }
    if (terminal) {
        std::fprintf(stderr, "\n");
    }
    // A line piped from a Windows file ends in \r, which would become part of the password
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return read;
}

// Makes an account an enabled administrator with a new password, for when the administrator's
// password is lost. Running it needs a shell on the board, which is what keeps it safe.
int recoverAdmin(const std::string& configPath, const std::string& username) {
    Logger::init(Logger::Level::Warning, "");

    Config config;
    if (!config.load(configPath)) {
        std::fprintf(stderr, "Couldn't load %s, so the database can't be found\n", configPath.c_str());
        return EXIT_FAILURE;
    }
    Database database(config.databasePath());
    if (!database.initialize()) {
        std::fprintf(stderr, "Couldn't open the database at %s\n", config.databasePath().c_str());
        return EXIT_FAILURE;
    }
    security::UserManager users(database);
    if (!users.initialize()) {
        std::fprintf(stderr, "Couldn't read the accounts\n");
        return EXIT_FAILURE;
    }

    std::string password;
    std::string again;
    if (!readSecretLine("New password: ", password)) {
        std::fprintf(stderr, "No password given\n");
        return EXIT_FAILURE;
    }
    if (isatty(STDIN_FILENO) && (!readSecretLine("Again: ", again) || again != password)) {
        std::fprintf(stderr, "The passwords don't match\n");
        return EXIT_FAILURE;
    }

    const std::string problem = users.recoverAdmin(username, password);
    if (!problem.empty()) {
        std::fprintf(stderr, "%s\n", problem.c_str());
        return EXIT_FAILURE;
    }
    std::printf("%s is an enabled administrator with the new password.\n", username.c_str());
    return EXIT_SUCCESS;
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
    std::printf("  --recover-admin <username>\n");
    std::printf("                       Make this account an enabled administrator with a new\n");
    std::printf("                       password, read from stdin, then exit. For a lost password.\n");
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
    std::string recoverUsername;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-c") == 0 || std::strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) {
                configPath = argv[++i];
            } else {
                std::fprintf(stderr, "Error: -c/--config requires an argument\n");
                return EXIT_FAILURE;
            }
        } else if (std::strcmp(argv[i], "--recover-admin") == 0) {
            if (i + 1 < argc) {
                recoverUsername = argv[++i];
            } else {
                std::fprintf(stderr, "Error: --recover-admin requires a username\n");
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

    if (!recoverUsername.empty()) {
        return recoverAdmin(configPath, recoverUsername);
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
            LOG_INFO("Stopped by signal %d", g_signalNumber.load());
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
