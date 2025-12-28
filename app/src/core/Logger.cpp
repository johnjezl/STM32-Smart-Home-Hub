/**
 * SmartHub Logger Implementation
 */

#include <smarthub/core/Logger.hpp>
#include <ctime>
#include <cstring>
#include <iostream>

namespace smarthub {

std::unique_ptr<Logger> Logger::s_instance;
std::once_flag Logger::s_onceFlag;

void Logger::init(Level minLevel, const std::string& logFile) {
    std::call_once(s_onceFlag, [&]() {
        s_instance = std::unique_ptr<Logger>(new Logger());
        s_instance->m_minLevel = minLevel;

        if (!logFile.empty()) {
            s_instance->m_logFile.open(logFile, std::ios::app);
            if (!s_instance->m_logFile.is_open()) {
                std::fprintf(stderr, "Warning: Could not open log file: %s\n", logFile.c_str());
            }
        }

        s_instance->m_initialized = true;
    });
}

Logger& Logger::instance() {
    // If not initialized, create with defaults
    if (!s_instance) {
        init(Level::Info, "");
    }
    return *s_instance;
}

Logger::~Logger() {
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
}

void Logger::log(Level level, const char* file, int line, const char* fmt, ...) {
    if (level < m_minLevel) {
        return;
    }

    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    writeLog(level, file, line, buffer);
}

void Logger::writeLog(Level level, const char* file, int line, const char* message) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Get current time
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);

    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);

    // Extract just the filename from path
    const char* filename = file;
    const char* lastSlash = std::strrchr(file, '/');
    if (lastSlash) {
        filename = lastSlash + 1;
    }

    // Format: [TIMESTAMP] LEVEL filename:line: message
    char logLine[2200];
    std::snprintf(logLine, sizeof(logLine), "[%s] %s %s:%d: %s\n",
                  timestamp, levelString(level), filename, line, message);

    // Write to stderr
    std::fputs(logLine, stderr);

    // Write to log file if open
    if (m_logFile.is_open()) {
        m_logFile << logLine;
        m_logFile.flush();
    }
}

const char* Logger::levelString(Level level) const {
    switch (level) {
        case Level::Debug:   return "DEBUG";
        case Level::Info:    return "INFO ";
        case Level::Warning: return "WARN ";
        case Level::Error:   return "ERROR";
        default:             return "?????";
    }
}

} // namespace smarthub
