#pragma once
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct AppMeta {
    std::wstring displayName;
    std::wstring lastMode;
};

struct AppUsageRow {
    std::wstring exePath;
    std::wstring displayName;
    int todaySec = 0;
    int weekSec = 0;
    int monthSec = 0;
    std::wstring mode;
};

class StatsStore {
public:
    explicit StatsStore(std::filesystem::path path);

    bool Load();
    bool Save() const;
    void AddUsage(const std::wstring& date, const std::wstring& exePath, const std::wstring& displayName, int sec, const std::wstring& mode);
    int QueryDay(const std::wstring& date, const std::wstring& exePath) const;
    int QueryWeek7d(const std::wstring& todayDate, const std::wstring& exePath) const;
    int QueryMonth(const std::wstring& yyyyMm, const std::wstring& exePath) const;

    std::vector<AppUsageRow> BuildTopRows(const std::wstring& todayDate, const std::wstring& yyyyMm, const std::vector<std::wstring>& mustInclude, int topN) const;
    const std::unordered_map<std::wstring, AppMeta>& AppInfo() const { return appInfo_; }

private:
    std::filesystem::path path_;
    mutable std::mutex mutex_;
    std::map<std::wstring, std::unordered_map<std::wstring, int>> dayStats_;
    std::unordered_map<std::wstring, AppMeta> appInfo_;
};

std::wstring CurrentDateYmd();
std::wstring CurrentMonthYm();
std::vector<std::wstring> Last7Days(const std::wstring& todayDate);
