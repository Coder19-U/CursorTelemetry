#include "Logger.h"

#include <Windows.h>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {
std::wstring TimestampNow() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::wstringstream ss;
    ss << std::setfill(L'0')
       << st.wYear << L'-' << std::setw(2) << st.wMonth << L'-' << std::setw(2) << st.wDay
       << L' ' << std::setw(2) << st.wHour << L':' << std::setw(2) << st.wMinute << L':' << std::setw(2)
       << st.wSecond;
    return ss.str();
}
}

Logger::Logger(const std::filesystem::path& logPath) : logPath_(logPath) {}

void Logger::Log(const std::wstring& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::wofstream file(logPath_, std::ios::app);
    if (!file.is_open()) {
        return;
    }
    file << L"[" << TimestampNow() << L"] " << message << L"\n";
}
