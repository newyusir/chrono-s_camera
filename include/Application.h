#pragma once

#include <string>
#include <vector>
#include <windows.h>

#include "CaptureSession.h"
#include "ConfigManager.h"
#include "HotkeyManager.h"
#include "SettingsWindow.h"
#include "WebProcessor.h"

class Application {
public:
    explicit Application(HINSTANCE instance);
    ~Application();

    bool Initialize(int nCmdShow);
    int Run();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void OnCreate();
    void OnSize();
    void OnCommand(WPARAM wParam);
    void ToggleCaptureMode();
    void EnterCaptureMode();
    void ExitCaptureMode();
    void HandleCaptureRequest();
    void StartProcessing(const std::vector<std::wstring>& capturedFiles);
    void HandleWebError(const std::wstring& message);
    void UpdateStatus(const std::wstring& text);
    void OpenOutputFolder();
    void ShowSettingsDialog();
    void ClearSessionDirectory();
    std::wstring BuildIdleStatus() const;
    void LoadWindowIcon();
    std::wstring FindIconAsset() const;
    std::wstring MakeAbsolutePath(const std::wstring& relative) const;
    void EnsureDirectories();
    bool CopyImagesToClipboard(const std::vector<std::wstring>& files);
    void EnsureUIFont();

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND statusStatic_ = nullptr;
    HFONT uiFont_ = nullptr;
    HICON appIconLarge_ = nullptr;
    HICON appIconSmall_ = nullptr;

    std::wstring baseDirectory_;
    std::wstring outputDirectory_;
    std::wstring sessionDirectory_;

    ConfigManager configManager_;
    AppConfig config_{};

    HotkeyManager hotkeyManager_;
    CaptureSession captureSession_;
    WebProcessor webProcessor_;

    bool captureModeActive_ = false;
    bool processingInFlight_ = false;
    std::vector<std::wstring> currentCapturedFiles_;
};
