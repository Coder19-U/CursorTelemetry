#include "Limits.h"

#include <Windows.h>
#include <fstream>
#include <regex>
#include <sstream>

namespace {
std::wstring Escape(const std::wstring& s) {
    std::wstring out;
    for (auto ch : s) {
        if (ch == L'\\' || ch == L'"') {
            out.push_back(L'\\');
        }
        out.push_back(ch);
    }
    return out;
}

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
}

LimitsStore::LimitsStore(std::filesystem::path path) : path_(std::move(path)) {}

bool LimitsStore::Load() {
    limits_.clear();
    std::wifstream file(path_);
    if (!file.is_open()) {
        Save();
        return true;
    }
    std::wstringstream ss;
    ss << file.rdbuf();
    const std::wstring txt = ss.str();

    std::wregex re(LR"("([^"]+)"\s*:\s*\{\s*"seconds"\s*:\s*(\d+)\s*,\s*"period"\s*:\s*"(day|month)"\s*\})");
    std::wsregex_iterator begin(txt.begin(), txt.end(), re), end;
    for (auto it = begin; it != end; ++it) {
        AppLimit lim;
        lim.seconds = std::stoi((*it)[2].str());
        lim.period = ((*it)[3].str() == L"month") ? LimitPeriod::Month : LimitPeriod::Day;
        limits_[(*it)[1].str()] = lim;
    }
    return true;
}

bool LimitsStore::Save() const {
    std::wstringstream ss;
    ss << L"{\n";
    bool first = true;
    for (const auto& [exe, lim] : limits_) {
        if (!first) {
            ss << L",\n";
        }
        first = false;
        ss << L"  \"" << Escape(exe) << L"\": { \"seconds\": " << lim.seconds << L", \"period\": \""
           << (lim.period == LimitPeriod::Month ? L"month" : L"day") << L"\" }";
    }
    ss << L"\n}\n";
    return AtomicWrite(path_, ss.str());
}

bool LimitsStore::SetLimit(const std::wstring& exePath, const std::wstring& spec) {
    AppLimit lim;
    if (!ParseSpec(spec, lim)) {
        return false;
    }
    limits_[exePath] = lim;
    return Save();
}

void LimitsStore::RemoveLimit(const std::wstring& exePath) {
    limits_.erase(exePath);
    Save();
}

std::optional<AppLimit> LimitsStore::GetLimit(const std::wstring& exePath) const {
    const auto it = limits_.find(exePath);
    if (it == limits_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool LimitsStore::ParseSpec(const std::wstring& spec, AppLimit& outLimit) {
    std::wregex re(LR"((\d+)([smh])/(day|month))");
    std::wsmatch m;
    if (!std::regex_match(spec, m, re)) {
        return false;
    }
    int value = std::stoi(m[1].str());
    wchar_t unit = m[2].str()[0];
    int mult = 1;
    if (unit == L'm') mult = 60;
    if (unit == L'h') mult = 3600;
    outLimit.seconds = value * mult;
    outLimit.period = (m[3].str() == L"month") ? LimitPeriod::Month : LimitPeriod::Day;
    return true;
}

std::wstring LimitsStore::FormatLimit(const AppLimit& lim) {
    int sec = lim.seconds;
    std::wstringstream ss;
    if (sec % 3600 == 0) {
        ss << sec / 3600 << L"h";
    }
    else if (sec % 60 == 0) {
        ss << sec / 60 << L"m";
    }
    else {
        ss << sec << L"s";
    }
    ss << L"/" << (lim.period == LimitPeriod::Month ? L"month" : L"day");
    return ss.str();
}
