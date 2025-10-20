#include "SettingsWindow.h"

#include <string>

#include "HotkeyUtils.h"

namespace {
constexpr int kWindowWidth = 380;
constexpr int kWindowHeight = 300;
constexpr int kPadding = 16;
constexpr int kLabelHeight = 20;
constexpr int kButtonWidth = 100;
constexpr int kButtonHeight = 28;

constexpr int IDC_LABEL_CURRENT = 1001;
constexpr int IDC_BUTTON_CHANGE = 1002;
constexpr int IDC_LABEL_INSTRUCTION = 1003;
constexpr int IDC_BUTTON_SAVE = 1004;
constexpr int IDC_BUTTON_CANCEL = 1005;

bool IsKeyDownMessage(WPARAM wParam) {
    return wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
}

bool IsKeyUpMessage(WPARAM wParam) {
    return wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
}

} // namespace

SettingsWindow* SettingsWindow::activeListener_ = nullptr;

std::optional<HotkeyConfig> SettingsWindow::Show(HINSTANCE instance, HWND parent, const HotkeyConfig& currentConfig) {
    SettingsWindow window(instance, parent, currentConfig);
    if (!window.Create()) {
        return std::nullopt;
    }

    MSG msg;
    while (window.hwnd_) {
        const BOOL result = GetMessage(&msg, nullptr, 0, 0);
        if (result <= 0) {
            break;
        }
        if (!IsDialogMessage(window.hwnd_, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (window.accepted_) {
        return window.workingConfig_;
    }
    return std::nullopt;
}

SettingsWindow::SettingsWindow(HINSTANCE instance, HWND parent, const HotkeyConfig& currentConfig)
    : instance_(instance), parent_(parent), workingConfig_(currentConfig) {}

bool SettingsWindow::Create() {
    const wchar_t className[] = L"ReceiptSettingsWindow";
    WNDCLASS wc = {};
    wc.lpfnWndProc = &SettingsWindow::WndProc;
    wc.hInstance = instance_;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClass(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    hwnd_ = CreateWindowExW(
        0,
        className,
        L"Capture Hotkey Settings",
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kWindowWidth,
        kWindowHeight,
        parent_,
        nullptr,
        instance_,
        this);

    if (!hwnd_) {
        return false;
    }
    if (parent_) {
        EnableWindow(parent_, FALSE);
    }
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    return true;
}

void SettingsWindow::PopulateControls() {
    EnsureUIFont();

    const int labelWidth = kWindowWidth - (kPadding * 2);
    labelHotkey_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                   kPadding, kPadding, labelWidth, kLabelHeight,
                                   hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LABEL_CURRENT)), instance_, nullptr);

    instructionLabel_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                        kPadding, kPadding + kLabelHeight + 8,
                                        labelWidth, (kLabelHeight * 2) + 16,
                                        hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LABEL_INSTRUCTION)), instance_, nullptr);

    changeButton_ = CreateWindowExW(0, L"BUTTON", L"Change...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    kPadding, kPadding + (kLabelHeight * 2) + 44,
                                    150, kButtonHeight,
                                    hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BUTTON_CHANGE)), instance_, nullptr);

    const int buttonsTop = kWindowHeight - kPadding - kButtonHeight - 16;
    HWND saveButton = CreateWindowExW(0, L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                      kWindowWidth - kPadding - (kButtonWidth * 2) - 12, buttonsTop,
                                      kButtonWidth, kButtonHeight,
                                      hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BUTTON_SAVE)), instance_, nullptr);
    HWND cancelButton = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        kWindowWidth - kPadding - kButtonWidth, buttonsTop,
                                        kButtonWidth, kButtonHeight,
                                        hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BUTTON_CANCEL)), instance_, nullptr);

    if (font_) {
        SendMessageW(labelHotkey_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        SendMessageW(instructionLabel_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        SendMessageW(changeButton_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        SendMessageW(saveButton, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        SendMessageW(cancelButton, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }

    UpdateHotkeyLabel();
    UpdateInstruction(L"Click \"Change\" and press the desired key combination.");
}

void SettingsWindow::OnCommand(WPARAM wParam) {
    const int id = LOWORD(wParam);
    switch (id) {
        case IDC_BUTTON_CHANGE:
            StartListening();
            break;
        case IDC_BUTTON_SAVE:
            accepted_ = true;
            DestroyWindow(hwnd_);
            break;
        case IDC_BUTTON_CANCEL:
            DestroyWindow(hwnd_);
            break;
        default:
            break;
    }
}

void SettingsWindow::OnClose() {
    StopListening();
    DestroyWindow(hwnd_);
}

void SettingsWindow::EnsureUIFont() {
    if (font_) {
        return;
    }
    HDC screen = GetDC(nullptr);
    const int dpi = screen ? GetDeviceCaps(screen, LOGPIXELSY) : 96;
    if (screen) {
        ReleaseDC(nullptr, screen);
    }
    const int logicalHeight = -MulDiv(10, dpi, 72);
    font_ = CreateFontW(logicalHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (!font_) {
        font_ = CreateFontW(logicalHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"MS Shell Dlg 2");
    }
}

void SettingsWindow::UpdateHotkeyLabel() {
    if (!labelHotkey_) {
        return;
    }
    const std::wstring hotkeyText = hotkey::Describe(workingConfig_);
    std::wstring label = L"Current hotkey: ";
    if (hotkeyText.empty()) {
        label += L"(not set)";
    } else {
        label += hotkeyText;
    }
    SetWindowTextW(labelHotkey_, label.c_str());
}

void SettingsWindow::UpdateInstruction(const std::wstring& text) {
    if (instructionLabel_) {
        SetWindowTextW(instructionLabel_, text.c_str());
    }
}

void SettingsWindow::StartListening() {
    if (listening_) {
        StopListening();
    }
    listeningHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, nullptr, 0);
    if (!listeningHook_) {
        UpdateInstruction(L"Failed to start hotkey capture.");
        return;
    }
    activeListener_ = this;
    listening_ = true;
    if (changeButton_) {
        EnableWindow(changeButton_, FALSE);
    }
    ResetCaptureTracking();
    UpdateInstruction(L"Press the desired hotkey, then release to finish. Click Cancel to abort.");
}

void SettingsWindow::StopListening() {
    if (listeningHook_) {
        UnhookWindowsHookEx(listeningHook_);
        listeningHook_ = nullptr;
    }
    if (activeListener_ == this) {
        activeListener_ = nullptr;
    }
    listening_ = false;
    if (changeButton_) {
        EnableWindow(changeButton_, TRUE);
    }
    ResetCaptureTracking();
}

void SettingsWindow::ResetCaptureTracking() {
    pressedKeys_.clear();
    capturedPrimary_ = 0;
    primaryReleased_ = false;
    capturedWinLeft_ = false;
    capturedWinRight_ = false;
    capturedCtrlLeft_ = false;
    capturedCtrlRight_ = false;
    capturedAltLeft_ = false;
    capturedAltRight_ = false;
    capturedShiftLeft_ = false;
    capturedShiftRight_ = false;
}

void SettingsWindow::HandleHookEvent(WPARAM wParam, const KBDLLHOOKSTRUCT* data) {
    if (!listening_ || !data) {
        return;
    }

    const UINT vk = data->vkCode;
    if (IsKeyDownMessage(wParam)) {
        if (pressedKeys_.insert(vk).second) {
            switch (vk) {
                case VK_LWIN:
                    capturedWinLeft_ = true;
                    break;
                case VK_RWIN:
                    capturedWinRight_ = true;
                    break;
                case VK_LCONTROL:
                    capturedCtrlLeft_ = true;
                    break;
                case VK_RCONTROL:
                    capturedCtrlRight_ = true;
                    break;
                case VK_LMENU:
                    capturedAltLeft_ = true;
                    break;
                case VK_RMENU:
                    capturedAltRight_ = true;
                    break;
                case VK_MENU:
                    capturedAltLeft_ = true;
                    capturedAltRight_ = true;
                    break;
                default:
                    break;
            }

            if (vk == VK_LSHIFT) {
                capturedShiftLeft_ = true;
            } else if (vk == VK_RSHIFT) {
                capturedShiftRight_ = true;
            }

            if (!IsCoreModifier(vk)) {
                if (capturedPrimary_ == 0) {
                    capturedPrimary_ = vk;
                    primaryReleased_ = false;
                } else if (IsShiftKey(capturedPrimary_) && !IsShiftKey(vk) && !IsCoreModifier(vk)) {
                    capturedPrimary_ = vk;
                    primaryReleased_ = false;
                }
            }
        }
    } else if (IsKeyUpMessage(wParam)) {
        pressedKeys_.erase(vk);
        if (vk == capturedPrimary_) {
            primaryReleased_ = true;
        }

        if (pressedKeys_.empty() && capturedPrimary_ != 0 && primaryReleased_) {
            if (ApplyCapturedHotkey()) {
                StopListening();
            } else {
                ResetCaptureTracking();
                UpdateInstruction(L"Please include a non-modifier key.");
            }
        }
    }
}

bool SettingsWindow::IsCoreModifier(UINT vk) {
    switch (vk) {
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_CONTROL:
        case VK_LMENU:
        case VK_RMENU:
        case VK_MENU:
        case VK_LWIN:
        case VK_RWIN:
            return true;
        default:
            return false;
    }
}

bool SettingsWindow::IsShiftKey(UINT vk) {
    return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT;
}

bool SettingsWindow::ApplyCapturedHotkey() {
    if (capturedPrimary_ == 0 || IsCoreModifier(capturedPrimary_)) {
        return false;
    }

    HotkeyConfig updated = workingConfig_;
    updated.primaryKey = capturedPrimary_;
    updated.requireWin = capturedWinLeft_ || capturedWinRight_;
    updated.requireCtrl = capturedCtrlLeft_ || capturedCtrlRight_;
    updated.requireAlt = capturedAltLeft_ || capturedAltRight_;

    const bool shiftUsed = capturedShiftLeft_ || capturedShiftRight_;
    const bool primaryIsShift = IsShiftKey(capturedPrimary_);
    updated.requireShift = shiftUsed && !primaryIsShift;

    if (!shiftUsed) {
        updated.shiftMode = HotkeyConfig::ShiftMode::Any;
    } else if (capturedShiftLeft_ && !capturedShiftRight_) {
        updated.shiftMode = HotkeyConfig::ShiftMode::LeftOnly;
    } else if (capturedShiftRight_ && !capturedShiftLeft_) {
        updated.shiftMode = HotkeyConfig::ShiftMode::RightOnly;
    } else {
        updated.shiftMode = HotkeyConfig::ShiftMode::Any;
    }

    workingConfig_ = updated;
    UpdateHotkeyLabel();
    UpdateInstruction(L"Captured new hotkey. Click Save to apply.");
    return true;
}

LRESULT CALLBACK SettingsWindow::KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code < 0) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
    auto listener = activeListener_;
    if (!listener) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
    listener->HandleHookEvent(wParam, reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam));
    return 1;
}

LRESULT CALLBACK SettingsWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto create = reinterpret_cast<LPCREATESTRUCT>(lParam);
        auto that = static_cast<SettingsWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(that));
        that->hwnd_ = hwnd;
        return TRUE;
    }

    auto that = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!that) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_CREATE:
            that->PopulateControls();
            return 0;
        case WM_COMMAND:
            that->OnCommand(wParam);
            return 0;
        case WM_CLOSE:
            that->OnClose();
            return 0;
        case WM_DESTROY:
            that->StopListening();
            if (that->font_) {
                DeleteObject(that->font_);
                that->font_ = nullptr;
            }
            if (that->parent_) {
                EnableWindow(that->parent_, TRUE);
                SetForegroundWindow(that->parent_);
            }
            that->hwnd_ = nullptr;
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}
