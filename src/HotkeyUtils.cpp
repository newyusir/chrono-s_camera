#include "HotkeyUtils.h"

#include <vector>
#include <windows.h>

namespace {

bool IsExtendedKey(UINT vk) {
    switch (vk) {
        case VK_PRIOR:
        case VK_NEXT:
        case VK_END:
        case VK_HOME:
        case VK_INSERT:
        case VK_DELETE:
        case VK_DIVIDE:
        case VK_RCONTROL:
        case VK_RMENU:
        case VK_LWIN:
        case VK_RWIN:
        case VK_APPS:
        case VK_RIGHT:
        case VK_LEFT:
        case VK_UP:
        case VK_DOWN:
            return true;
        default:
            return false;
    }
}

bool IsShiftKey(UINT vk) {
    return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT;
}

std::wstring JoinWithPlus(const std::vector<std::wstring>& parts) {
    if (parts.empty()) {
        return L"";
    }
    std::wstring result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            result += L" + ";
        }
        result += parts[i];
    }
    return result;
}

} // namespace

namespace hotkey {

std::wstring DescribeKey(UINT vk) {
    switch (vk) {
        case VK_LSHIFT:
            return L"Left Shift";
        case VK_RSHIFT:
            return L"Right Shift";
        case VK_SHIFT:
            return L"Shift";
        case VK_LCONTROL:
            return L"Left Ctrl";
        case VK_RCONTROL:
            return L"Right Ctrl";
        case VK_CONTROL:
            return L"Ctrl";
        case VK_LMENU:
            return L"Left Alt";
        case VK_RMENU:
            return L"Right Alt";
        case VK_MENU:
            return L"Alt";
        case VK_LWIN:
        case VK_RWIN:
            return L"Win";
        default:
            break;
    }

    const UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    if (scan == 0) {
        wchar_t buffer[16] = {};
        swprintf_s(buffer, L"0x%X", vk);
        return buffer;
    }

    LONG lParam = static_cast<LONG>(scan) << 16;
    if (IsExtendedKey(vk)) {
        lParam |= (1 << 24);
    }
    if (vk == VK_RSHIFT) {
        lParam |= (1 << 24);
    }

    wchar_t name[64] = {};
    const int length = GetKeyNameTextW(lParam, name, static_cast<int>(sizeof(name) / sizeof(name[0])));
    if (length > 0) {
        return std::wstring(name, name + length);
    }

    wchar_t buffer[16] = {};
    swprintf_s(buffer, L"0x%X", vk);
    return buffer;
}

std::wstring Describe(const HotkeyConfig& config) {
    std::vector<std::wstring> parts;
    if (config.requireWin) {
        parts.emplace_back(L"Win");
    }
    if (config.requireCtrl) {
        parts.emplace_back(L"Ctrl");
    }
    if (config.requireAlt) {
        parts.emplace_back(L"Alt");
    }

    bool includeShift = false;
    switch (config.primaryKey) {
        case VK_LSHIFT:
            parts.emplace_back(L"Left Shift");
            includeShift = false;
            break;
        case VK_RSHIFT:
            parts.emplace_back(L"Right Shift");
            includeShift = false;
            break;
        case VK_SHIFT:
            parts.emplace_back(L"Shift");
            includeShift = false;
            break;
        default:
            includeShift = config.requireShift || config.shiftMode != HotkeyConfig::ShiftMode::Any;
            break;
    }

    if (includeShift) {
        switch (config.shiftMode) {
            case HotkeyConfig::ShiftMode::LeftOnly:
                parts.emplace_back(L"Left Shift");
                break;
            case HotkeyConfig::ShiftMode::RightOnly:
                parts.emplace_back(L"Right Shift");
                break;
            default:
                parts.emplace_back(L"Shift");
                break;
        }
    }

    if (!(config.primaryKey == VK_LSHIFT || config.primaryKey == VK_RSHIFT || config.primaryKey == VK_SHIFT)) {
        parts.emplace_back(DescribeKey(config.primaryKey));
    }
    return JoinWithPlus(parts);
}

} // namespace hotkey
