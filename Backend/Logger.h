#pragma once
#include <string>
#include <pthread.h>

// =============================================================================
// Logger.h — Thread-Safe File Logger using POSIX System Calls
// OS Concept: Direct system calls (open, write, close) instead of C++ streams.
// This demonstrates explicit kernel interaction for I/O operations.
// =============================================================================

enum class LogLevel {
    DEBUG,
    INFO,
    TRADE,   // Completed trade executions
    WARNING,
    ERROR_L
};

class Logger {
public:
    static Logger& getInstance();

    // Opens log file using open() system call
    bool init(const std::string& tradeLogPath,
              const std::string& systemLogPath);

    // Logs a trade execution (goes to trade log)
    void logTrade(const std::string& message);

    // Logs system events (goes to system log)
    void logSystem(LogLevel level, const std::string& message);

    // Logs to both files
    void log(LogLevel level, const std::string& message);

    // Suppress stdout/stderr echo (useful for tests). File logging continues.
    void setQuiet(bool quiet) { m_quiet = quiet; }

    void close();

    ~Logger();

private:
    Logger();
    Logger(const Logger&) = delete;

    int         m_tradeFd;    // File descriptor for trade log
    int         m_systemFd;   // File descriptor for system log
    pthread_mutex_t m_mutex;  // Protect concurrent writes
    bool        m_quiet = false;

    std::string levelStr(LogLevel level);
    std::string timestamp();

    // Uses write() system call directly
    void writeToFd(int fd, const std::string& msg);
};
