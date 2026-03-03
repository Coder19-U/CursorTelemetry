#pragma once
#include "Config.h"
#include "Limits.h"
#include "StatsStore.h"
#include "WindowTracker.h"

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

class Dialogs;
class IdleDetector;
class Logger;
class ProcessControl;

class ConsoleUI {
public:
    ConsoleUI(const AppSettings& settings, StatsStore& stats, LimitsStore& limits, ProcessControl& processControl,
              Dialogs& dialogs, const IdleDetector& idle, Logger& logger);

    void PrintTop(const std::wstring& period, bool remember);
    void CommandLoop(std::atomic<bool>& stopFlag);
    std::wstring ExeByIndex(int index) const;

private:
    const AppSettings& settings_;
    StatsStore& stats_;
    LimitsStore& limits_;
    ProcessControl& processControl_;
    Dialogs& dialogs_;
    const IdleDetector& idle_;
    Logger& logger_;

    mutable std::mutex mapMutex_;
    std::unordered_map<int, std::wstring> lastIndexMap_;

    static std::wstring FormatDuration(int sec);
    static std::wstring ShortPath(const std::wstring& exe);
    void PrintHelp() const;
};
