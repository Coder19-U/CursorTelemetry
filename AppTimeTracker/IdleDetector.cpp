#include "IdleDetector.h"

#include <Windows.h>

int IdleDetector::GetIdleSeconds() const {
    LASTINPUTINFO info{};
    info.cbSize = sizeof(info);
    if (!GetLastInputInfo(&info)) {
        return 0;
    }
    const DWORD now = GetTickCount();
    return static_cast<int>((now - info.dwTime) / 1000);
}

bool IdleDetector::IsIdle(int thresholdSec) const {
    return GetIdleSeconds() >= thresholdSec;
}
