#include "Config.h"
#include "ConsoleUI.h"
#include "Dialogs.h"
#include "IdleDetector.h"
#include "Limits.h"
#include "Logger.h"
#include "ProcessControl.h"
#include "StatsStore.h"
#include "WindowTracker.h"

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

namespace {
std::wstring ModeToText(UsageMode mode) {
    if (mode == UsageMode::ActiveForeground) return L"AF";
    if (mode == UsageMode::Media) return L"MEDIA";
    return L"—";
}

std::wstring CurrentExePath() {
    wchar_t buf[MAX_PATH * 4] = {};
    DWORD sz = static_cast<DWORD>(std::size(buf));
    if (QueryFullProcessImageNameW(GetCurrentProcess(), 0, buf, &sz)) {
        return std::wstring(buf, sz);
    }
    return L"";
}
}

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    ConfigManager cfgMgr;
    RuntimeConfig cfg = cfgMgr.Load(argc, argv);

    Logger logger(cfg.dataDir / L"events.log");
    logger.Log(L"AppTimeTracker started");

    StatsStore stats(cfg.dataDir / L"stats.json");
    LimitsStore limits(cfg.dataDir / L"limits.json");
    stats.Load();
    limits.Load();

    IdleDetector idle;
    WindowTracker tracker;
    Dialogs dialogs;
    ProcessControl processControl(logger);
    ConsoleUI ui(cfg.settings, stats, limits, processControl, dialogs, idle, logger);

    std::atomic<bool> stopFlag = false;
    std::thread inputThread([&]() { ui.CommandLoop(stopFlag); });

    const auto selfExe = CurrentExePath();
    auto nextPrint = std::chrono::steady_clock::now() + std::chrono::seconds(cfg.settings.printIntervalSec);
    auto nextSave = std::chrono::steady_clock::now() + std::chrono::seconds(30);

    while (!stopFlag.load()) {
        const bool isIdle = idle.IsIdle(cfg.settings.idleThresholdSec);
        if (!isIdle) {
            auto active = tracker.GetEligibleActiveApp(selfExe);
            if (active.has_value()) {
                const auto today = CurrentDateYmd();
                stats.AddUsage(today, active->exePath, active->displayName, cfg.settings.tickSec, ModeToText(active->mode));

                auto lim = limits.GetLimit(active->exePath);
                if (lim.has_value()) {
                    const int todaySec = stats.QueryDay(today, active->exePath);
                    const int monthSec = stats.QueryMonth(CurrentMonthYm(), active->exePath);
                    const int used = (lim->period == LimitPeriod::Day) ? todaySec : monthSec;
                    if (used > lim->seconds) {
                        logger.Log(L"Limit exceeded: " + active->exePath);
                        const std::wstring msg = L"Лимит времени превышен. Пожалуйста, сохраните данные. Через " +
                            std::to_wstring(cfg.settings.graceSec) + L" сек. будет выполнено закрытие приложения.";
                        bool yes = dialogs.ShowConfirmClose(L"Превышен лимит", msg);
                        if (yes) {
                            std::this_thread::sleep_for(std::chrono::seconds(cfg.settings.graceSec));
                            if (!idle.IsIdle(cfg.settings.idleThresholdSec)) {
                                processControl.StopProcessByExe(active->exePath, cfg.settings.softCloseSec, cfg.settings.allowHardKill);
                            }
                        }
                    }
                }
            }
            // TODO: MEDIA usage mode via WASAPI session inspection could be added here.
        }

        auto now = std::chrono::steady_clock::now();
        if (now >= nextPrint) {
            ui.PrintTop(L"day", true);
            nextPrint = now + std::chrono::seconds(cfg.settings.printIntervalSec);
        }
        if (now >= nextSave) {
            stats.Save();
            limits.Save();
            nextSave = now + std::chrono::seconds(30);
        }

        std::this_thread::sleep_for(std::chrono::seconds(cfg.settings.tickSec));
    }

    if (inputThread.joinable()) {
        if (GetConsoleWindow()) {
            // Ensure potentially blocked stdin loop can terminate after exit command.
        }
        inputThread.join();
    }

    stats.Save();
    limits.Save();
    cfgMgr.SaveSettings();
    logger.Log(L"AppTimeTracker stopped");
    return 0;
}
