#pragma once

#include <optional>
#include <string>
#include <unordered_set>
#include <windows.h>

#include "ConfigManager.h"

class SettingsWindow {
public:
    static std::optional<HotkeyConfig> Show(HINSTANCE instance, HWND parent, const HotkeyConfig& currentConfig);

private:
    SettingsWindow(HINSTANCE instance, HWND parent, const HotkeyConfig& currentConfig);
    bool Create();
    void PopulateControls();
    void OnCommand(WPARAM wParam);
    void OnClose();

    void EnsureUIFont();
    void UpdateHotkeyLabel();
    void UpdateInstruction(const std::wstring& text);
    void StartListening();
    void StopListening();
    void ResetCaptureTracking();
    void HandleHookEvent(WPARAM wParam, const KBDLLHOOKSTRUCT* data);
    bool ApplyCapturedHotkey();

    static bool IsCoreModifier(UINT vk);
    static bool IsShiftKey(UINT vk);
    static LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    static SettingsWindow* activeListener_;

    HINSTANCE instance_ = nullptr;
    HWND parent_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND labelHotkey_ = nullptr;
    HWND instructionLabel_ = nullptr;
    HWND changeButton_ = nullptr;
    HFONT font_ = nullptr;

    HHOOK listeningHook_ = nullptr;
    bool listening_ = false;
    std::unordered_set<UINT> pressedKeys_;
    UINT capturedPrimary_ = 0;
    bool primaryReleased_ = false;
    bool capturedWinLeft_ = false;
    bool capturedWinRight_ = false;
    bool capturedCtrlLeft_ = false;
    bool capturedCtrlRight_ = false;
    bool capturedAltLeft_ = false;
    bool capturedAltRight_ = false;
    bool capturedShiftLeft_ = false;
    bool capturedShiftRight_ = false;

    HotkeyConfig workingConfig_{};
    bool accepted_ = false;
};
