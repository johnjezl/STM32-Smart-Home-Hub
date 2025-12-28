/**
 * SmartHub Logger
 *
 * Lightweight logging system for embedded Linux.
 * Supports multiple log levels, file output, and timestamped entries.
 */
#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <cstdio>
#include <cstdarg>

namespace smarthub {

class Logger {
public:
    enum class Level {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3
    };

    /**
     * Initialize the global logger
     * @param minLevel Minimum log level to output
     * @param logFile Path to log file (empty for stderr only)
     */
    static void init(Level minLevel = Level::Info, const std::string& logFile = "");

    /**
     * Get the global logger instance
     */
    static Logger& instance();

    /**
     * Log a message at the specified level
     */
    void log(Level level, const char* file, int line, const char* fmt, ...);

    /**
     * Set minimum log level
     */
    void setLevel(Level level) { m_minLevel = level; }

    /**
     * Get current log level
     */
    Level level() const { return m_minLevel; }

    /**
     * Check if a level is enabled
     */
    bool isEnabled(Level level) const { return level >= m_minLevel; }

    // Destructor needs to be public for unique_ptr
    ~Logger();

private:
    Logger() = default;
    void writeLog(Level level, const char* file, int line, const char* message);
    const char* levelString(Level level) const;

    Level m_minLevel = Level::Info;
    std::ofstream m_logFile;
    std::mutex m_mutex;
    bool m_initialized = false;

    static std::unique_ptr<Logger> s_instance;
    static std::once_flag s_onceFlag;
};

// Convenience macros
#define LOG_DEBUG(...) \
    do { if (smarthub::Logger::instance().isEnabled(smarthub::Logger::Level::Debug)) \
        smarthub::Logger::instance().log(smarthub::Logger::Level::Debug, __FILE__, __LINE__, __VA_ARGS__); \
    } while(0)

#define LOG_INFO(...) \
    smarthub::Logger::instance().log(smarthub::Logger::Level::Info, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_WARN(...) \
    smarthub::Logger::instance().log(smarthub::Logger::Level::Warning, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_ERROR(...) \
    smarthub::Logger::instance().log(smarthub::Logger::Level::Error, __FILE__, __LINE__, __VA_ARGS__)

} // namespace smarthub
