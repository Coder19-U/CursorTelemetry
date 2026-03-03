#pragma once
#include <Windows.h>

#include <optional>
#include <string>

enum class UsageMode {
    None,
    ActiveForeground,
    Media
};

struct ActiveAppInfo {
    DWORD pid = 0;
    std::wstring exePath;
    std::wstring displayName;
    UsageMode mode = UsageMode::None;
};

class WindowTracker {
public:
    std::optional<ActiveAppInfo> GetEligibleActiveApp(const std::wstring& selfExePath) const;

private:
    bool IsWindowEligible(HWND hwnd) const;
    std::wstring QueryExePath(DWORD pid) const;
    std::wstring QueryDisplayName(const std::wstring& exePath) const;
};
