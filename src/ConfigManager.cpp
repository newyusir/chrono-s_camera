#include "ConfigManager.h"

#include "Utility.h"

#include <array>
#include <sstream>
#include <cwctype>
#include <windows.h>

namespace {

struct KeyName {
    const wchar_t* name;
    UINT vk;
};

constexpr std::array<KeyName, 37> kNamedKeys = { {
    {L"VK_BACK", VK_BACK},
    {L"VK_TAB", VK_TAB},
    {L"VK_RETURN", VK_RETURN},
    {L"VK_SHIFT", VK_SHIFT},
    {L"VK_CONTROL", VK_CONTROL},
    {L"VK_MENU", VK_MENU},
    {L"VK_PAUSE", VK_PAUSE},
    {L"VK_CAPITAL", VK_CAPITAL},
    {L"VK_ESCAPE", VK_ESCAPE},
    {L"VK_SPACE", VK_SPACE},
    {L"VK_PRIOR", VK_PRIOR},
    {L"VK_NEXT", VK_NEXT},
    {L"VK_END", VK_END},
    {L"VK_HOME", VK_HOME},
    {L"VK_LEFT", VK_LEFT},
    {L"VK_UP", VK_UP},
    {L"VK_RIGHT", VK_RIGHT},
    {L"VK_DOWN", VK_DOWN},
    {L"VK_INSERT", VK_INSERT},
    {L"VK_DELETE", VK_DELETE},
    {L"VK_LWIN", VK_LWIN},
    {L"VK_RWIN", VK_RWIN},
    {L"VK_APPS", VK_APPS},
    {L"VK_NUMPAD0", VK_NUMPAD0},
    {L"VK_NUMPAD1", VK_NUMPAD1},
    {L"VK_NUMPAD2", VK_NUMPAD2},
    {L"VK_NUMPAD3", VK_NUMPAD3},
    {L"VK_NUMPAD4", VK_NUMPAD4},
    {L"VK_NUMPAD5", VK_NUMPAD5},
    {L"VK_NUMPAD6", VK_NUMPAD6},
    {L"VK_NUMPAD7", VK_NUMPAD7},
    {L"VK_NUMPAD8", VK_NUMPAD8},
    {L"VK_NUMPAD9", VK_NUMPAD9},
    {L"VK_MULTIPLY", VK_MULTIPLY},
    {L"VK_ADD", VK_ADD},
    {L"VK_SUBTRACT", VK_SUBTRACT},
    {L"VK_RSHIFT", VK_RSHIFT}
} };

UINT VkFromString(const std::wstring& token) {
    auto lower = util::ToLower(token);
    for (const auto& item : kNamedKeys) {
        if (lower == util::ToLower(item.name)) {
            return item.vk;
        }
    }
    if (token.size() == 1) {
        wchar_t ch = towupper(token[0]);
        SHORT scan = VkKeyScanW(ch);
        if (scan != -1) {
            return static_cast<UINT>(scan & 0xFF);
        }
    }
    if (token.size() >= 2 && (token[0] == L'F' || token[0] == L'f')) {
        int fnIndex = std::stoi(token.substr(1));
        if (fnIndex >= 1 && fnIndex <= 24) {
            return VK_F1 + fnIndex - 1;
        }
    }
    return VK_RSHIFT;
}

std::wstring StringFromVk(UINT vk) {
    for (const auto& item : kNamedKeys) {
        if (item.vk == vk) {
            return item.name;
        }
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        std::wstringstream ss;
        ss << L"F" << (vk - VK_F1 + 1);
        return ss.str();
    }
    if ((vk >= L'A' && vk <= L'Z') || (vk >= L'0' && vk <= L'9')) {
        wchar_t buffer[2] = { static_cast<wchar_t>(vk), L'\0' };
        return buffer;
    }
    std::wstringstream unknown;
    unknown << L"0x" << std::hex << vk;
    return unknown.str();
}

HotkeyConfig::ShiftMode ShiftModeFromString(const std::wstring& value) {
    const auto lower = util::ToLower(value);
    if (lower == L"leftonly") {
        return HotkeyConfig::ShiftMode::LeftOnly;
    }
    if (lower == L"rightonly") {
        return HotkeyConfig::ShiftMode::RightOnly;
    }
    return HotkeyConfig::ShiftMode::Any;
}

std::wstring ShiftModeToString(HotkeyConfig::ShiftMode mode) {
    switch (mode) {
        case HotkeyConfig::ShiftMode::LeftOnly:
            return L"LeftOnly";
        case HotkeyConfig::ShiftMode::RightOnly:
            return L"RightOnly";
        default:
            return L"Any";
    }
}

} // namespace

ConfigManager::ConfigManager(const std::wstring& baseDirectory)
    : baseDirectory_(baseDirectory) {
    configPath_ = util::JoinPath(baseDirectory_, L"config.ini");
}

bool ConfigManager::Load(AppConfig& outConfig) {
    // Use defaults
    AppConfig config = outConfig;

    wchar_t buffer[128] = {};
    GetPrivateProfileStringW(L"hotkey", L"primaryKey", StringFromVk(config.hotkey.primaryKey).c_str(), buffer, 128, configPath_.c_str());
    config.hotkey.primaryKey = VkFromString(buffer);

    const auto requireWin = GetPrivateProfileIntW(L"hotkey", L"requireWin", config.hotkey.requireWin ? 1 : 0, configPath_.c_str());
    const auto requireCtrl = GetPrivateProfileIntW(L"hotkey", L"requireCtrl", config.hotkey.requireCtrl ? 1 : 0, configPath_.c_str());
    const auto requireAlt = GetPrivateProfileIntW(L"hotkey", L"requireAlt", config.hotkey.requireAlt ? 1 : 0, configPath_.c_str());
    const auto requireShift = GetPrivateProfileIntW(L"hotkey", L"requireShift", config.hotkey.requireShift ? 1 : 0, configPath_.c_str());
    GetPrivateProfileStringW(L"hotkey", L"shiftMode", ShiftModeToString(config.hotkey.shiftMode).c_str(), buffer, 128, configPath_.c_str());

    config.hotkey.requireWin = (requireWin != 0);
    config.hotkey.requireCtrl = (requireCtrl != 0);
    config.hotkey.requireAlt = (requireAlt != 0);
    config.hotkey.requireShift = (requireShift != 0);
    config.hotkey.shiftMode = ShiftModeFromString(buffer);

    GetPrivateProfileStringW(L"paths", L"outputDirectory", config.paths.outputDirectory.c_str(), buffer, 128, configPath_.c_str());
    if (buffer[0] != L'\0') {
        config.paths.outputDirectory = buffer;
    }
    GetPrivateProfileStringW(L"paths", L"sessionDirectory", config.paths.sessionDirectory.c_str(), buffer, 128, configPath_.c_str());
    if (buffer[0] != L'\0') {
        config.paths.sessionDirectory = buffer;
    }

    int scrollsPerCapture = GetPrivateProfileIntW(L"capture", L"scrollsPerCapture", static_cast<int>(config.scrollsPerCapture), configPath_.c_str());
    if (scrollsPerCapture <= 0) {
        scrollsPerCapture = 1;
    }
    config.scrollsPerCapture = static_cast<UINT>(scrollsPerCapture);

    outConfig = config;
    return true;
}

bool ConfigManager::Save(const AppConfig& config) {
    const std::wstring primaryKey = StringFromVk(config.hotkey.primaryKey);
    if (!WritePrivateProfileStringW(L"hotkey", L"primaryKey", primaryKey.c_str(), configPath_.c_str())) {
        return false;
    }
    if (!WritePrivateProfileStringW(L"hotkey", L"requireWin", config.hotkey.requireWin ? L"1" : L"0", configPath_.c_str())) {
        return false;
    }
    if (!WritePrivateProfileStringW(L"hotkey", L"requireCtrl", config.hotkey.requireCtrl ? L"1" : L"0", configPath_.c_str())) {
        return false;
    }
    if (!WritePrivateProfileStringW(L"hotkey", L"requireAlt", config.hotkey.requireAlt ? L"1" : L"0", configPath_.c_str())) {
        return false;
    }
    if (!WritePrivateProfileStringW(L"hotkey", L"requireShift", config.hotkey.requireShift ? L"1" : L"0", configPath_.c_str())) {
        return false;
    }
    const auto shiftMode = ShiftModeToString(config.hotkey.shiftMode);
    if (!WritePrivateProfileStringW(L"hotkey", L"shiftMode", shiftMode.c_str(), configPath_.c_str())) {
        return false;
    }
    if (!WritePrivateProfileStringW(L"paths", L"outputDirectory", config.paths.outputDirectory.c_str(), configPath_.c_str())) {
        return false;
    }
    if (!WritePrivateProfileStringW(L"paths", L"sessionDirectory", config.paths.sessionDirectory.c_str(), configPath_.c_str())) {
        return false;
    }
    wchar_t scrollBuffer[16] = {};
    const UINT clampedScrolls = config.scrollsPerCapture == 0 ? 1u : config.scrollsPerCapture;
    swprintf_s(scrollBuffer, L"%u", static_cast<unsigned int>(clampedScrolls));
    if (!WritePrivateProfileStringW(L"capture", L"scrollsPerCapture", scrollBuffer, configPath_.c_str())) {
        return false;
    }
    return true;
}
