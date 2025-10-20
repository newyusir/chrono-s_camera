#pragma once

#include <string>
#include <vector>
#include <windows.h>

class CaptureSession {
public:
    CaptureSession();

    bool Begin(HWND targetWindow, const std::wstring& baseDirectory);
    std::wstring CaptureNext();
    std::vector<std::wstring> End();
    bool IsActive() const { return active_; }
    HWND TargetWindow() const { return targetWindow_; }
    const std::wstring& SessionRoot() const { return sessionRoot_; }

private:
    bool CaptureWindowToFile(HWND hwnd, const std::wstring& filePath);
    std::wstring NextCaptureFilename() const;

    HWND targetWindow_ = nullptr;
    std::wstring baseDirectory_;
    std::wstring sessionRoot_;
    std::wstring rawDirectory_;
    std::vector<std::wstring> capturedFiles_;
    size_t captureIndex_ = 0;
    bool active_ = false;
};
