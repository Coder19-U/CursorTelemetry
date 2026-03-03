#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

enum class LimitPeriod {
    Day,
    Month
};

struct AppLimit {
    int seconds = 0;
    LimitPeriod period = LimitPeriod::Day;
};

class LimitsStore {
public:
    explicit LimitsStore(std::filesystem::path path);

    bool Load();
    bool Save() const;
    bool SetLimit(const std::wstring& exePath, const std::wstring& spec);
    void RemoveLimit(const std::wstring& exePath);
    std::optional<AppLimit> GetLimit(const std::wstring& exePath) const;
    const std::unordered_map<std::wstring, AppLimit>& All() const { return limits_; }

    static bool ParseSpec(const std::wstring& spec, AppLimit& outLimit);
    static std::wstring FormatLimit(const AppLimit& lim);

private:
    std::filesystem::path path_;
    std::unordered_map<std::wstring, AppLimit> limits_;
};
