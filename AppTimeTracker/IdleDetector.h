#pragma once

class IdleDetector {
public:
    int GetIdleSeconds() const;
    bool IsIdle(int thresholdSec) const;
};
