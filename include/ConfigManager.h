#pragma once

#include <string>
#include <windows.h>

struct HotkeyConfig {
    UINT primaryKey = VK_RSHIFT;
    bool requireWin = true;
    bool requireCtrl = false;
    bool requireAlt = false;
    bool requireShift = false;
    enum class ShiftMode {
        Any,
        LeftOnly,
        RightOnly
    };
    ShiftMode shiftMode = ShiftMode::RightOnly;
};

struct AppPaths {
    std::wstring outputDirectory;
    std::wstring sessionDirectory;
};

struct AppConfig {
    HotkeyConfig hotkey;
    AppPaths paths;
    UINT scrollsPerCapture = 3;
};

class ConfigManager {
public:
    explicit ConfigManager(const std::wstring& baseDirectory);

    bool Load(AppConfig& outConfig);
    bool Save(const AppConfig& config);

    const std::wstring& GetConfigPath() const { return configPath_; }

private:
    std::wstring baseDirectory_;
    std::wstring configPath_;
};
