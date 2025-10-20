#include "HotkeyManager.h"

#include <chrono>
#include <unordered_set>

HotkeyManager* HotkeyManager::instance_ = nullptr;

namespace {
bool IsModifierKey(UINT vk) {
    switch (vk) {
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
        case VK_LWIN:
        case VK_RWIN:
            return true;
        default:
            return false;
    }
}
}

HotkeyManager::HotkeyManager() = default;
HotkeyManager::~HotkeyManager() {
    Shutdown();
}

bool HotkeyManager::Initialize(const HotkeyConfig& config, UINT scrollsPerCapture, ToggleCallback onToggle, CaptureRequest onCaptureRequest) {
    if (instance_ != nullptr) {
        return false;
    }
    config_ = config;
    onToggle_ = std::move(onToggle);
    onCaptureRequest_ = std::move(onCaptureRequest);
    SetScrollsPerCapture(scrollsPerCapture);

    keyboardHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, nullptr, 0);
    mouseHook_ = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, nullptr, 0);
    if (!keyboardHook_ || !mouseHook_) {
        Shutdown();
        return false;
    }
    instance_ = this;
    return true;
}

void HotkeyManager::UpdateConfig(const HotkeyConfig& config) {
    config_ = config;
    ResetCombinationLatch();
    keysDown_.clear();
}

void HotkeyManager::Shutdown() {
    if (keyboardHook_) {
        UnhookWindowsHookEx(keyboardHook_);
        keyboardHook_ = nullptr;
    }
    if (mouseHook_) {
        UnhookWindowsHookEx(mouseHook_);
        mouseHook_ = nullptr;
    }
    instance_ = nullptr;
    keysDown_.clear();
}

void HotkeyManager::SetCaptureMode(bool enabled) {
    captureMode_.store(enabled);
    pendingScrolls_ = 0;
}

void HotkeyManager::SetScrollsPerCapture(UINT count) {
    if (count == 0) {
        count = 1;
    }
    scrollsPerCapture_ = count;
    pendingScrolls_ = 0;
}

LRESULT CALLBACK HotkeyManager::KeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code < 0) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
    if (instance_) {
        instance_->HandleKeyboardEvent(wParam, reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam));
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK HotkeyManager::MouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code < 0) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
    if (instance_) {
        instance_->HandleMouseEvent(wParam, reinterpret_cast<MSLLHOOKSTRUCT*>(lParam));
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void HotkeyManager::HandleKeyboardEvent(WPARAM wParam, const KBDLLHOOKSTRUCT* data) {
    if (!data) {
        return;
    }

    const bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    const bool isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

    if (isKeyDown) {
        keysDown_.insert(data->vkCode);
        if (data->vkCode == VK_LSHIFT || data->vkCode == VK_RSHIFT) {
            keysDown_.insert(VK_SHIFT);
        }
        if (data->vkCode == VK_LCONTROL || data->vkCode == VK_RCONTROL) {
            keysDown_.insert(VK_CONTROL);
        }
        if (data->vkCode == VK_LMENU || data->vkCode == VK_RMENU) {
            keysDown_.insert(VK_MENU);
        }
        if (CheckToggleCondition(data->vkCode, true)) {
            if (onToggle_) {
                onToggle_();
            }
        }
    } else if (isKeyUp) {
        keysDown_.erase(data->vkCode);
        if (data->vkCode == VK_LSHIFT || data->vkCode == VK_RSHIFT) {
            keysDown_.erase(VK_SHIFT);
        }
        if (data->vkCode == VK_LCONTROL || data->vkCode == VK_RCONTROL) {
            keysDown_.erase(VK_CONTROL);
        }
        if (data->vkCode == VK_LMENU || data->vkCode == VK_RMENU) {
            keysDown_.erase(VK_MENU);
        }
        if (IsModifierKey(data->vkCode) || data->vkCode == config_.primaryKey) {
            ResetCombinationLatch();
        }
    }
}

void HotkeyManager::HandleMouseEvent(WPARAM wParam, const MSLLHOOKSTRUCT* data) {
    if (!data) {
        return;
    }
    if (!captureMode_.load()) {
        return;
    }
    if (wParam == WM_MOUSEWHEEL) {
        const auto delta = static_cast<SHORT>(HIWORD(data->mouseData));
        if (delta < 0) {
            pendingScrolls_ += 1;
            if (pendingScrolls_ >= scrollsPerCapture_) {
                pendingScrolls_ = 0;
                if (onCaptureRequest_) {
                    onCaptureRequest_();
                }
            }
        } else if (delta > 0) {
            pendingScrolls_ = 0;
        }
    }
}

bool HotkeyManager::CheckToggleCondition(UINT vkCode, bool isKeyDown) {
    if (!isKeyDown) {
        return false;
    }
    if (consumedCombination_.load()) {
        return false;
    }

    const bool mainPressed = keysDown_.count(config_.primaryKey) > 0;
    bool winOk = !config_.requireWin || keysDown_.count(VK_LWIN) > 0 || keysDown_.count(VK_RWIN) > 0;
    bool ctrlOk = !config_.requireCtrl || keysDown_.count(VK_CONTROL) > 0 || keysDown_.count(VK_LCONTROL) > 0 || keysDown_.count(VK_RCONTROL) > 0;
    bool altOk = !config_.requireAlt || keysDown_.count(VK_MENU) > 0 || keysDown_.count(VK_LMENU) > 0 || keysDown_.count(VK_RMENU) > 0;
    bool shiftOk = true;

    if (config_.requireShift) {
        shiftOk = keysDown_.count(VK_SHIFT) > 0 || keysDown_.count(VK_LSHIFT) > 0 || keysDown_.count(VK_RSHIFT) > 0;
    }
    if (config_.shiftMode == HotkeyConfig::ShiftMode::LeftOnly) {
        shiftOk = keysDown_.count(VK_LSHIFT) > 0;
    } else if (config_.shiftMode == HotkeyConfig::ShiftMode::RightOnly) {
        shiftOk = keysDown_.count(VK_RSHIFT) > 0;
    }

    const bool triggered = mainPressed && winOk && ctrlOk && altOk && shiftOk;
    if (triggered) {
        consumedCombination_.store(true);
    }
    return triggered;
}

void HotkeyManager::ResetCombinationLatch() {
    consumedCombination_.store(false);
}
