#include "StatsStore.h"

#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>

namespace {
bool AtomicWrite(const std::filesystem::path& path, const std::wstring& content) {
    const auto tmp = path.wstring() + L".tmp";
    {
        std::wofstream out(tmp, std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out << content;
    }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == TRUE;
}

SYSTEMTIME ParseYmd(const std::wstring& ymd) {
    SYSTEMTIME st{};
    swscanf_s(ymd.c_str(), L"%hu-%hu-%hu", &st.wYear, &st.wMonth, &st.wDay);
    return st;
}

std::wstring FormatYmd(const SYSTEMTIME& st) {
    std::wstringstream ss;
    ss << std::setfill(L'0') << st.wYear << L"-" << std::setw(2) << st.wMonth << L"-" << std::setw(2) << st.wDay;
    return ss.str();
}
}

std::wstring CurrentDateYmd() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    return FormatYmd(st);
}

std::wstring CurrentMonthYm() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::wstringstream ss;
    ss << std::setfill(L'0') << st.wYear << L"-" << std::setw(2) << st.wMonth;
    return ss.str();
}

std::vector<std::wstring> Last7Days(const std::wstring& todayDate) {
    SYSTEMTIME st = ParseYmd(todayDate);
    FILETIME ft{};
    SystemTimeToFileTime(&st, &ft);
    ULARGE_INTEGER ul{};
    ul.LowPart = ft.dwLowDateTime;
    ul.HighPart = ft.dwHighDateTime;

    std::vector<std::wstring> dates;
    constexpr ULONGLONG dayTicks = 24ull * 60ull * 60ull * 10000000ull;
    for (int i = 0; i < 7; ++i) {
        ULARGE_INTEGER cur = ul;
        cur.QuadPart -= static_cast<ULONGLONG>(i) * dayTicks;
        FILETIME dft{};
        dft.dwLowDateTime = cur.LowPart;
        dft.dwHighDateTime = cur.HighPart;
        SYSTEMTIME dst{};
        FileTimeToSystemTime(&dft, &dst);
        dates.push_back(FormatYmd(dst));
    }
    return dates;
}

StatsStore::StatsStore(std::filesystem::path path) : path_(std::move(path)) {}

bool StatsStore::Load() {
    std::lock_guard<std::mutex> lock(mutex_);
    dayStats_.clear();
    appInfo_.clear();

    std::wifstream file(path_);
    if (!file.is_open()) {
        Save();
        return true;
    }
    std::wstringstream ss;
    ss << file.rdbuf();
    const std::wstring txt = ss.str();

    std::wregex re(LR"("(\d{4}-\d{2}-\d{2})\|([^"]+)"\s*:\s*\{\s*"seconds"\s*:\s*(\d+)\s*,\s*"name"\s*:\s*"([^"]*)"\s*,\s*"mode"\s*:\s*"([^"]*)"\s*\})");
    std::wsregex_iterator begin(txt.begin(), txt.end(), re), end;
    for (auto it = begin; it != end; ++it) {
        const auto date = (*it)[1].str();
        const auto exe = (*it)[2].str();
        dayStats_[date][exe] = std::stoi((*it)[3].str());
        appInfo_[exe] = AppMeta{(*it)[4].str(), (*it)[5].str()};
    }
    return true;
}

bool StatsStore::Save() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::wstringstream ss;
    ss << L"{\n";
    bool first = true;
    for (const auto& [date, apps] : dayStats_) {
        for (const auto& [exe, sec] : apps) {
            if (!first) ss << L",\n";
            first = false;
            auto metaIt = appInfo_.find(exe);
            const auto name = (metaIt != appInfo_.end() ? metaIt->second.displayName : L"");
            const auto mode = (metaIt != appInfo_.end() ? metaIt->second.lastMode : L"—");
            ss << L"  \"" << date << L"|" << exe << L"\": { \"seconds\": " << sec
               << L", \"name\": \"" << name << L"\", \"mode\": \"" << mode << L"\" }";
        }
    }
    ss << L"\n}\n";
    return AtomicWrite(path_, ss.str());
}

void StatsStore::AddUsage(const std::wstring& date, const std::wstring& exePath, const std::wstring& displayName, int sec, const std::wstring& mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    dayStats_[date][exePath] += sec;
    appInfo_[exePath] = AppMeta{displayName, mode};
}

int StatsStore::QueryDay(const std::wstring& date, const std::wstring& exePath) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto d = dayStats_.find(date);
    if (d == dayStats_.end()) return 0;
    auto a = d->second.find(exePath);
    return (a == d->second.end()) ? 0 : a->second;
}

int StatsStore::QueryWeek7d(const std::wstring& todayDate, const std::wstring& exePath) const {
    int total = 0;
    for (const auto& d : Last7Days(todayDate)) {
        total += QueryDay(d, exePath);
    }
    return total;
}

int StatsStore::QueryMonth(const std::wstring& yyyyMm, const std::wstring& exePath) const {
    std::lock_guard<std::mutex> lock(mutex_);
    int total = 0;
    for (const auto& [date, apps] : dayStats_) {
        if (date.rfind(yyyyMm, 0) != 0) continue;
        auto it = apps.find(exePath);
        if (it != apps.end()) total += it->second;
    }
    return total;
}

std::vector<AppUsageRow> StatsStore::BuildTopRows(const std::wstring& todayDate, const std::wstring& yyyyMm, const std::vector<std::wstring>& mustInclude, int topN) const {
    std::set<std::wstring> all;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [date, apps] : dayStats_) {
            for (const auto& [exe, _] : apps) {
                all.insert(exe);
            }
        }
    }
    for (const auto& m : mustInclude) all.insert(m);

    std::vector<AppUsageRow> rows;
    for (const auto& exe : all) {
        AppUsageRow row;
        row.exePath = exe;
        auto it = appInfo_.find(exe);
        row.displayName = (it != appInfo_.end() ? it->second.displayName : std::filesystem::path(exe).filename().wstring());
        row.mode = (it != appInfo_.end() ? it->second.lastMode : L"—");
        row.todaySec = QueryDay(todayDate, exe);
        row.weekSec = QueryWeek7d(todayDate, exe);
        row.monthSec = QueryMonth(yyyyMm, exe);
        rows.push_back(row);
    }

    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
        if (a.todaySec != b.todaySec) return a.todaySec > b.todaySec;
        return a.exePath < b.exePath;
    });

    if (static_cast<int>(rows.size()) > topN) {
        rows.resize(topN);
    }

    for (const auto& exe : mustInclude) {
        auto it = std::find_if(rows.begin(), rows.end(), [&](const auto& r) { return r.exePath == exe; });
        if (it == rows.end()) {
            AppUsageRow row;
            row.exePath = exe;
            auto m = appInfo_.find(exe);
            row.displayName = (m != appInfo_.end() ? m->second.displayName : std::filesystem::path(exe).filename().wstring());
            row.mode = (m != appInfo_.end() ? m->second.lastMode : L"—");
            row.todaySec = QueryDay(todayDate, exe);
            row.weekSec = QueryWeek7d(todayDate, exe);
            row.monthSec = QueryMonth(yyyyMm, exe);
            rows.push_back(row);
        }
    }
    return rows;
}
