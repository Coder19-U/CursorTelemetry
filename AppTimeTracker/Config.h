#pragma once
#include <filesystem>
#include <string>
#include <vector>

struct AppSettings {
    int tickSec = 2;
    int idleThresholdSec = 120;
    int printIntervalSec = 300;
    int softCloseSec = 60;
    int graceSec = 60;
    bool allowHardKill = true;
};

struct RuntimeConfig {
    AppSettings settings;
    std::filesystem::path dataDir;
};

class ConfigManager {
public:
    RuntimeConfig Load(int argc, wchar_t* argv[]);
    bool SaveSettings() const;
    const std::filesystem::path& SettingsPath() const { return settingsPath_; }

private:
    AppSettings settings_{};
    std::filesystem::path dataDir_;
    std::filesystem::path settingsPath_;

    void ResolveDataDir(const std::vector<std::wstring>& args);
    void LoadSettingsFromDisk();
    void ApplyCli(const std::vector<std::wstring>& args);
};
