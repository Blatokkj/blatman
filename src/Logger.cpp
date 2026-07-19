#include "Logger.hpp"
#include <iostream>

Logger::Logger(const fs::path& logDir) {
    if (!fs::exists(logDir)) {
        fs::create_directories(logDir);
    }

    auto now = std::chrono::system_clock::now();
    auto date = std::chrono::floor<std::chrono::days>(now);
    std::string filename = "blatman_" + std::format("{:%Y%m%d}", date) + ".log";
    logFile_ = logDir / filename;

    logStream_.open(logFile_, std::ios::app);
    if (!logStream_.is_open()) {
        throw std::runtime_error("Falha ao abrir arquivo de log.");
    }
}

void Logger::logError(const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex_);
    std::string logMessage = "[" + getCurrentTimestamp() + "] [ERRO] " + message;
    logStream_ << logMessage << std::endl;
    logStream_.flush();
}

void Logger::logInfo(const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex_);
    std::string logMessage = "[" + getCurrentTimestamp() + "] [INFO] " + message;
    logStream_ << logMessage << std::endl;
    logStream_.flush();
}

std::string Logger::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}