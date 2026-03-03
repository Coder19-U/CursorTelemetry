#include "WindowTracker.h"

#include <Psapi.h>
#include <VersionHelpers.h>
#include <filesystem>
#include <vector>

bool WindowTracker::IsWindowEligible(HWND hwnd) const {
    if (!hwnd || IsIconic(hwnd) || !IsWindowVisible(hwnd)) {
        return false;
    }

    RECT rc{};
    if (!GetWindowRect(hwnd, &rc)) {
        return false;
    }

    HMONITOR mon = MonitorFromRect(&rc, MONITOR_DEFAULTTONULL);
    return mon != nullptr;
}

std::wstring WindowTracker::QueryExePath(DWORD pid) const {
    std::wstring result;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) {
        return result;
    }

    wchar_t path[MAX_PATH * 4] = {};
    DWORD size = static_cast<DWORD>(std::size(path));
    if (QueryFullProcessImageNameW(h, 0, path, &size)) {
        result.assign(path, size);
    }
    CloseHandle(h);
    return result;
}

std::wstring WindowTracker::QueryDisplayName(const std::wstring& exePath) const {
    DWORD dummy = 0;
    DWORD sz = GetFileVersionInfoSizeW(exePath.c_str(), &dummy);
    if (sz == 0) {
        return std::filesystem::path(exePath).filename().wstring();
    }

    std::vector<BYTE> data(sz);
    if (!GetFileVersionInfoW(exePath.c_str(), 0, sz, data.data())) {
        return std::filesystem::path(exePath).filename().wstring();
    }

    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    } *translate;
    UINT cbTranslate = 0;
    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<LPVOID*>(&translate), &cbTranslate) ||
        cbTranslate < sizeof(LANGANDCODEPAGE)) {
        return std::filesystem::path(exePath).filename().wstring();
    }

    wchar_t subBlock[64] = {};
    swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\FileDescription", translate[0].wLanguage, translate[0].wCodePage);
    LPVOID value = nullptr;
    UINT size = 0;
    if (VerQueryValueW(data.data(), subBlock, &value, &size) && value && size > 1) {
        return std::wstring(static_cast<wchar_t*>(value));
    }

    return std::filesystem::path(exePath).filename().wstring();
}

std::optional<ActiveAppInfo> WindowTracker::GetEligibleActiveApp(const std::wstring& selfExePath) const {
    HWND hwnd = GetForegroundWindow();
    if (!IsWindowEligible(hwnd)) {
        return std::nullopt;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        return std::nullopt;
    }

    auto exePath = QueryExePath(pid);
    if (exePath.empty()) {
        return std::nullopt;
    }

    if (_wcsicmp(exePath.c_str(), selfExePath.c_str()) == 0) {
        return std::nullopt;
    }

    ActiveAppInfo info;
    info.pid = pid;
    info.exePath = exePath;
    info.displayName = QueryDisplayName(exePath);
    info.mode = UsageMode::ActiveForeground;
    return info;
}
