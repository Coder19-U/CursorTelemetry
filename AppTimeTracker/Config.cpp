#include "Config.h"

#include <Windows.h>
#include <ShlObj.h>
#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

namespace {
std::wstring ReadFileToWString(const std::filesystem::path& path) {
    std::wifstream file(path);
    if (!file.is_open()) {
        return L"";
    }
    std::wstringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
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

int ExtractInt(const std::wstring& txt, const std::wstring& key, int fallback) {
    const std::wregex re(L"\"" + key + L"\"\\s*:\\s*(\\d+)");
    std::wsmatch m;
    if (std::regex_search(txt, m, re)) {
        return std::stoi(m[1].str());
    }
    return fallback;
}

bool ExtractBool(const std::wstring& txt, const std::wstring& key, bool fallback) {
    const std::wregex re(L"\"" + key + L"\"\\s*:\\s*(true|false)");
    std::wsmatch m;
    if (std::regex_search(txt, m, re)) {
        return m[1].str() == L"true";
    }
    return fallback;
}
}

RuntimeConfig ConfigManager::Load(int argc, wchar_t* argv[]) {
    std::vector<std::wstring> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    ResolveDataDir(args);
    std::filesystem::create_directories(dataDir_);
    settingsPath_ = dataDir_ / L"settings.json";

    LoadSettingsFromDisk();
    ApplyCli(args);
    SaveSettings();

    return RuntimeConfig{settings_, dataDir_};
}

bool ConfigManager::SaveSettings() const {
    std::wstringstream ss;
    ss << L"{\n"
       << L"  \"tickSec\": " << settings_.tickSec << L",\n"
       << L"  \"idleThresholdSec\": " << settings_.idleThresholdSec << L",\n"
       << L"  \"printIntervalSec\": " << settings_.printIntervalSec << L",\n"
       << L"  \"softCloseSec\": " << settings_.softCloseSec << L",\n"
       << L"  \"graceSec\": " << settings_.graceSec << L",\n"
       << L"  \"allowHardKill\": " << (settings_.allowHardKill ? L"true" : L"false") << L"\n"
       << L"}\n";
    return AtomicWrite(settingsPath_, ss.str());
}

void ConfigManager::ResolveDataDir(const std::vector<std::wstring>& args) {
    for (const auto& arg : args) {
        if (arg.rfind(L"--data-dir=", 0) == 0) {
            std::wstring p = arg.substr(11);
            if (!p.empty() && p.front() == L'"' && p.back() == L'"') {
                p = p.substr(1, p.size() - 2);
            }
            dataDir_ = p;
            return;
        }
    }

    PWSTR roamingPath = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roamingPath) == S_OK) {
        dataDir_ = std::filesystem::path(roamingPath) / L"AppTimeTracker";
        CoTaskMemFree(roamingPath);
    }
    else {
        dataDir_ = std::filesystem::current_path() / L"AppTimeTrackerData";
    }
}

void ConfigManager::LoadSettingsFromDisk() {
    if (!std::filesystem::exists(settingsPath_)) {
        return;
    }
    const std::wstring txt = ReadFileToWString(settingsPath_);
    settings_.tickSec = ExtractInt(txt, L"tickSec", settings_.tickSec);
    settings_.idleThresholdSec = ExtractInt(txt, L"idleThresholdSec", settings_.idleThresholdSec);
    settings_.printIntervalSec = ExtractInt(txt, L"printIntervalSec", settings_.printIntervalSec);
    settings_.softCloseSec = ExtractInt(txt, L"softCloseSec", settings_.softCloseSec);
    settings_.graceSec = ExtractInt(txt, L"graceSec", settings_.graceSec);
    settings_.allowHardKill = ExtractBool(txt, L"allowHardKill", settings_.allowHardKill);
}

void ConfigManager::ApplyCli(const std::vector<std::wstring>& args) {
    for (const auto& arg : args) {
        if (arg.rfind(L"--tick=", 0) == 0) {
            settings_.tickSec = std::max(1, std::stoi(arg.substr(7)));
        }
        else if (arg.rfind(L"--idle=", 0) == 0) {
            settings_.idleThresholdSec = std::max(1, std::stoi(arg.substr(7)));
        }
        else if (arg == L"--no-hard-kill") {
            settings_.allowHardKill = false;
        }
    }
}
