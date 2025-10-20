#pragma once

#include <atomic>
#include <functional>
#include <unordered_set>
#include <windows.h>

#include "ConfigManager.h"

class HotkeyManager {
public:
    using ToggleCallback = std::function<void()>;
    using CaptureRequest = std::function<void()>;

    HotkeyManager();
    ~HotkeyManager();

    bool Initialize(const HotkeyConfig& config, UINT scrollsPerCapture, ToggleCallback onToggle, CaptureRequest onCaptureRequest);
    void UpdateConfig(const HotkeyConfig& config);
    void Shutdown();

    bool IsCaptureModeEnabled() const { return captureMode_.load(); }
    void SetCaptureMode(bool enabled);
    void SetScrollsPerCapture(UINT count);

private:
    static LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MouseProc(int code, WPARAM wParam, LPARAM lParam);

    void HandleKeyboardEvent(WPARAM wParam, const KBDLLHOOKSTRUCT* data);
    void HandleMouseEvent(WPARAM wParam, const MSLLHOOKSTRUCT* data);
    bool CheckToggleCondition(UINT vkCode, bool isKeyDown);
    void ResetCombinationLatch();

    HotkeyConfig config_{};
    ToggleCallback onToggle_{};
    CaptureRequest onCaptureRequest_{};

    static HotkeyManager* instance_;

    HHOOK keyboardHook_ = nullptr;
    HHOOK mouseHook_ = nullptr;

    std::unordered_set<UINT> keysDown_;
    std::atomic<bool> captureMode_{ false };
    std::atomic<bool> consumedCombination_{ false };
    UINT scrollsPerCapture_ = 1;
    UINT pendingScrolls_ = 0;
};
