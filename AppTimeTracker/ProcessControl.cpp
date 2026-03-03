#include "ProcessControl.h"

#include "Logger.h"

#include <TlHelp32.h>
#include <vector>

namespace {
struct EnumCtx {
    DWORD pid;
    std::vector<HWND> windows;
};

BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lp) {
    auto* ctx = reinterpret_cast<EnumCtx*>(lp);
    DWORD wpid = 0;
    GetWindowThreadProcessId(hwnd, &wpid);
    if (wpid == ctx->pid && GetWindow(hwnd, GW_OWNER) == nullptr) {
        ctx->windows.push_back(hwnd);
    }
    return TRUE;
}
}

ProcessControl::ProcessControl(Logger& logger) : logger_(logger) {}

bool ProcessControl::StopProcessByExe(const std::wstring& exePath, int softCloseSec, bool allowHardKill) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        logger_.Log(L"Stop: CreateToolhelp32Snapshot failed");
        return false;
    }

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    bool any = false;

    if (Process32FirstW(snap, &pe)) {
        do {
            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID);
            if (!h) continue;
            wchar_t buf[MAX_PATH * 4] = {};
            DWORD sz = static_cast<DWORD>(std::size(buf));
            if (!QueryFullProcessImageNameW(h, 0, buf, &sz) || _wcsicmp(buf, exePath.c_str()) != 0) {
                CloseHandle(h);
                continue;
            }
            any = true;

            EnumCtx ctx{ pe.th32ProcessID, {} };
            EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&ctx));
            for (HWND w : ctx.windows) {
                PostMessageW(w, WM_CLOSE, 0, 0);
            }

            const DWORD waitMs = static_cast<DWORD>(softCloseSec * 1000);
            DWORD wr = WaitForSingleObject(h, waitMs);
            if (wr == WAIT_TIMEOUT && allowHardKill) {
                logger_.Log(L"Stop: soft close timeout, killing " + exePath);
                TerminateProcess(h, 1);
            }
            else {
                logger_.Log(L"Stop: closed gracefully " + exePath);
            }
            CloseHandle(h);
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return any;
}
