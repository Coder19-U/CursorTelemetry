#pragma once
#include <Windows.h>

#include <string>

class Logger;

class ProcessControl {
public:
    explicit ProcessControl(Logger& logger);
    bool StopProcessByExe(const std::wstring& exePath, int softCloseSec, bool allowHardKill);

private:
    Logger& logger_;
};
