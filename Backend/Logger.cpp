#include "Logger.h"
#include <fcntl.h>      // open()
#include <unistd.h>     // write(), close()
#include <ctime>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <sys/stat.h>

// =============================================================================
// Logger.cpp — Explicit POSIX System Calls for File I/O
// System calls used: open(2), write(2), close(2)
// These bypass the C++ standard library to demonstrate direct OS interaction.
// =============================================================================

Logger::Logger() : m_tradeFd(-1), m_systemFd(-1) {
    pthread_mutex_init(&m_mutex, nullptr);
}

Logger::~Logger() {
    close();
    pthread_mutex_destroy(&m_mutex);
}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

bool Logger::init(const std::string& tradeLogPath,
                  const std::string& systemLogPath) {
    // --- SYSTEM CALL: open() ---
    // Flags: O_WRONLY (write-only), O_CREAT (create if not exists),
    //        O_APPEND (append to end), O_TRUNC (truncate existing)
    // Mode: 0644 = owner rw, group r, others r
    m_tradeFd = ::open(tradeLogPath.c_str(),
                       O_WRONLY | O_CREAT | O_TRUNC,
                       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    m_systemFd = ::open(systemLogPath.c_str(),
                        O_WRONLY | O_CREAT | O_TRUNC,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (m_tradeFd < 0 || m_systemFd < 0) {
        std::cerr << "[Logger] ERROR: Failed to open log files\n";
        return false;
    }

    std::string header = "=== SentinelTrade Log Started: " + timestamp() + " ===\n";
    writeToFd(m_tradeFd,  header);
    writeToFd(m_systemFd, header);
    return true;
}

void Logger::writeToFd(int fd, const std::string& msg) {
    if (fd < 0) return;
    // --- SYSTEM CALL: write() ---
    // Writes raw bytes directly to the file descriptor.
    // Returns number of bytes written; loop handles partial writes.
    const char* buf = msg.c_str();
    ssize_t     remaining = static_cast<ssize_t>(msg.size());
    ssize_t     written   = 0;

    while (remaining > 0) {
        ssize_t n = ::write(fd, buf + written, remaining);
        if (n <= 0) break;
        written   += n;
        remaining -= n;
    }
}

void Logger::logTrade(const std::string& message) {
    pthread_mutex_lock(&m_mutex);
    std::string line = "[" + timestamp() + "] [TRADE] " + message + "\n";
    writeToFd(m_tradeFd, line);
    // Also echo to stdout for real-time monitoring
    std::cout << line;
    pthread_mutex_unlock(&m_mutex);
}

void Logger::logSystem(LogLevel level, const std::string& message) {
    pthread_mutex_lock(&m_mutex);
    std::string line = "[" + timestamp() + "] [" + levelStr(level) + "] " + message + "\n";
    writeToFd(m_systemFd, line);
    if (level == LogLevel::WARNING || level == LogLevel::ERROR_L) {
        std::cerr << line;
    }
    pthread_mutex_unlock(&m_mutex);
}

void Logger::log(LogLevel level, const std::string& message) {
    logSystem(level, message);
}

void Logger::close() {
    // --- SYSTEM CALL: close() ---
    if (m_tradeFd >= 0)  { ::close(m_tradeFd);  m_tradeFd  = -1; }
    if (m_systemFd >= 0) { ::close(m_systemFd); m_systemFd = -1; }
}

std::string Logger::levelStr(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO ";
        case LogLevel::TRADE:   return "TRADE";
        case LogLevel::WARNING: return "WARN ";
        case LogLevel::ERROR_L: return "ERROR";
    }
    return "?????";
}

std::string Logger::timestamp() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    return std::string(buf);
}
