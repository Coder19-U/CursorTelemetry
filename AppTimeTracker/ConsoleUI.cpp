#include "ConsoleUI.h"

#include "Dialogs.h"
#include "IdleDetector.h"
#include "Logger.h"
#include "ProcessControl.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

ConsoleUI::ConsoleUI(const AppSettings& settings, StatsStore& stats, LimitsStore& limits, ProcessControl& processControl,
                     Dialogs& dialogs, const IdleDetector& idle, Logger& logger)
    : settings_(settings), stats_(stats), limits_(limits), processControl_(processControl), dialogs_(dialogs), idle_(idle), logger_(logger) {}

std::wstring ConsoleUI::FormatDuration(int sec) {
    int h = sec / 3600;
    int m = (sec % 3600) / 60;
    int s = sec % 60;
    std::wstringstream ss;
    ss << std::setfill(L'0') << std::setw(2) << h << L":" << std::setw(2) << m << L":" << std::setw(2) << s;
    return ss.str();
}

std::wstring ConsoleUI::ShortPath(const std::wstring& exe) {
    if (exe.size() <= 32) return exe;
    return exe.substr(0, 13) + L"..." + exe.substr(exe.size() - 16);
}

void ConsoleUI::PrintTop(const std::wstring& period, bool remember) {
    const auto today = CurrentDateYmd();
    const auto month = CurrentMonthYm();
    std::vector<std::wstring> forced;
    for (const auto& [exe, _] : limits_.All()) {
        forced.push_back(exe);
    }

    auto rows = stats_.BuildTopRows(today, month, forced, 10);
    if (period == L"week") {
        std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.weekSec > b.weekSec; });
        if (rows.size() > 10) rows.resize(10);
    }
    else if (period == L"month") {
        std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.monthSec > b.monthSec; });
        if (rows.size() > 10) rows.resize(10);
    }

    std::wcout << L"\n┌────┬──────────────────────────┬──────────────────────────────────┬──────────┬──────────┬──────────┬────────────┬──────────────┬──────┐\n";
    std::wcout << L"│ #  │ AppName                   │ Exe                              │ Today    │ Week(7d) │ Month    │ Limit      │ Remaining/Over│ Mode │\n";
    std::wcout << L"├────┼──────────────────────────┼──────────────────────────────────┼──────────┼──────────┼──────────┼────────────┼──────────────┼──────┤\n";

    std::unordered_map<int, std::wstring> tempMap;
    int idx = 1;
    for (const auto& row : rows) {
        auto lim = limits_.GetLimit(row.exePath);
        std::wstring limTxt = L"—";
        std::wstring rem = L"—";
        if (lim.has_value()) {
            limTxt = LimitsStore::FormatLimit(*lim);
            int used = (lim->period == LimitPeriod::Day) ? row.todaySec : row.monthSec;
            int delta = lim->seconds - used;
            rem = (delta >= 0 ? L"left " : L"over ") + FormatDuration(std::abs(delta));
        }

        tempMap[idx] = row.exePath;
        std::wcout << L"│ " << std::setw(2) << idx << L" │ " << std::left << std::setw(24) << row.displayName.substr(0, 24)
                   << L" │ " << std::setw(32) << ShortPath(row.exePath) << L" │ "
                   << std::setw(8) << FormatDuration(row.todaySec) << L" │ "
                   << std::setw(8) << FormatDuration(row.weekSec) << L" │ "
                   << std::setw(8) << FormatDuration(row.monthSec) << L" │ "
                   << std::setw(10) << limTxt << L" │ "
                   << std::setw(12) << rem << L" │ "
                   << std::setw(4) << row.mode << L" │\n";
        ++idx;
    }
    std::wcout << L"└────┴──────────────────────────┴──────────────────────────────────┴──────────┴──────────┴──────────┴────────────┴──────────────┴──────┘\n";

    if (remember) {
        std::lock_guard<std::mutex> lock(mapMutex_);
        lastIndexMap_ = std::move(tempMap);
    }
}

std::wstring ConsoleUI::ExeByIndex(int index) const {
    std::lock_guard<std::mutex> lock(mapMutex_);
    auto it = lastIndexMap_.find(index);
    if (it == lastIndexMap_.end()) return L"";
    return it->second;
}

void ConsoleUI::PrintHelp() const {
    std::wcout << L"AppTimeTracker - трекер времени использования приложений\n"
               << L"Команды:\n"
               << L"  help\n  day\n  week\n  month\n  stop N\n  text \"сообщение\"\n  lim N 3h/day\n  lim N 20m/month\n  nolim N\n  exit\n";
}

void ConsoleUI::CommandLoop(std::atomic<bool>& stopFlag) {
    PrintHelp();
    for (std::wstring line; !stopFlag.load() && std::getline(std::wcin, line); ) {
        if (line == L"help") {
            PrintHelp();
        }
        else if (line == L"day" || line == L"week" || line == L"month") {
            PrintTop(line, true);
        }
        else if (line.rfind(L"text ", 0) == 0) {
            std::wstring text = line.substr(5);
            if (!text.empty() && text.front() == L'"' && text.back() == L'"') text = text.substr(1, text.size() - 2);
            dialogs_.ShowInfo(L"Сообщение", text);
        }
        else if (line.rfind(L"stop ", 0) == 0) {
            int n = std::stoi(line.substr(5));
            const auto exe = ExeByIndex(n);
            if (exe.empty()) {
                std::wcout << L"Нет такого индекса в последней таблице\n";
                continue;
            }
            bool idle = idle_.IsIdle(settings_.idleThresholdSec);
            bool proceed = true;
            if (!idle) {
                proceed = dialogs_.ShowConfirmClose(L"Остановка приложения", L"Приложение будет закрыто. Сохраните данные перед закрытием.");
            }
            if (proceed) {
                logger_.Log(L"Manual stop requested: " + exe);
                processControl_.StopProcessByExe(exe, settings_.softCloseSec, settings_.allowHardKill);
            }
        }
        else if (line.rfind(L"lim ", 0) == 0) {
            std::wstringstream ss(line.substr(4));
            int idx = 0;
            std::wstring spec;
            ss >> idx >> spec;
            const auto exe = ExeByIndex(idx);
            if (exe.empty() || !limits_.SetLimit(exe, spec)) {
                std::wcout << L"Ошибка установки лимита\n";
            } else {
                std::wcout << L"Лимит сохранён\n";
            }
        }
        else if (line.rfind(L"nolim ", 0) == 0) {
            int n = std::stoi(line.substr(6));
            const auto exe = ExeByIndex(n);
            if (exe.empty()) {
                std::wcout << L"Нет такого индекса\n";
            }
            else {
                limits_.RemoveLimit(exe);
                std::wcout << L"Лимит снят\n";
            }
        }
        else if (line == L"exit") {
            stopFlag.store(true);
            break;
        }
        else {
            std::wcout << L"Неизвестная команда. help\n";
        }
    }
}
