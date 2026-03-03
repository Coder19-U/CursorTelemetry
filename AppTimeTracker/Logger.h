#pragma once
#include <filesystem>
#include <mutex>
#include <string>

class Logger {
public:
    explicit Logger(const std::filesystem::path& logPath);
    void Log(const std::wstring& message);

private:
    std::filesystem::path logPath_;
    std::mutex mutex_;
};
