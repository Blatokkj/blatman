#pragma once
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <mutex>

namespace fs = std::filesystem;

class Logger {
public:
    Logger(const fs::path& logDir);
    void logError(const std::string& message);
    void logInfo(const std::string& message);

private:
    fs::path logFile_;
    std::ofstream logStream_;
    mutable std::mutex logMutex_;

    std::string getCurrentTimestamp() const;
};