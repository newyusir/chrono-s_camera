#include "Application.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <shellapi.h>
#include <shlobj.h>
#include <vector>
#include <cstring>
#include <cstdint>

#include <wincodec.h>
#include <wrl/client.h>

#include "Utility.h"
#include "HotkeyUtils.h"
#include "resource.h"

namespace {
const wchar_t kWindowClassName[] = L"ReceiptFactorCaptureWindow";
const int kStatusControlId = 2001;
const int kMenuOpenOutput = 3001;
const int kMenuClearSessions = 3002;
const int kMenuSettings = 3003;

Microsoft::WRL::ComPtr<IWICImagingFactory> g_clipboardWicFactory;

bool EnsureClipboardFactory() {
    if (!g_clipboardWicFactory) {
        auto hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_clipboardWicFactory));
        if (FAILED(hr)) {
            g_clipboardWicFactory.Reset();
            return false;
        }
    }
    return true;
}

bool LoadImagePixels(const std::wstring& path, std::vector<BYTE>& pixels, UINT& width, UINT& height, UINT& stride, UINT targetSize = 0) {
    if (!EnsureClipboardFactory()) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    auto hr = g_clipboardWicFactory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapSource> source;
    if (targetSize > 0) {
        Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;
        hr = g_clipboardWicFactory->CreateBitmapScaler(&scaler);
        if (FAILED(hr)) {
            return false;
        }
        hr = scaler->Initialize(frame.Get(), targetSize, targetSize, WICBitmapInterpolationModeHighQualityCubic);
        if (FAILED(hr)) {
            return false;
        }
        hr = scaler.As(&source);
    } else {
        hr = frame.As(&source);
    }
    if (FAILED(hr) || !source) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = g_clipboardWicFactory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        return false;
    }

    hr = converter->Initialize(source.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        return false;
    }

    hr = converter->GetSize(&width, &height);
    if (FAILED(hr)) {
        return false;
    }

    stride = width * 4;
    const UINT bufferSize = stride * height;
    pixels.resize(bufferSize);

    hr = converter->CopyPixels(nullptr, stride, bufferSize, pixels.data());
    if (FAILED(hr)) {
        pixels.clear();
        return false;
    }
    return true;
}

bool CreateClipboardBitmaps(const std::wstring& path, HBITMAP& outBitmap, HGLOBAL& outDib) {
    outBitmap = nullptr;
    outDib = nullptr;

    UINT width = 0;
    UINT height = 0;
    UINT stride = 0;
    std::vector<BYTE> pixels;
    if (!LoadImagePixels(path, pixels, width, height, stride)) {
        return false;
    }

    BITMAPINFOHEADER header = {};
    header.biSize = sizeof(BITMAPINFOHEADER);
    header.biWidth = static_cast<LONG>(width);
    header.biHeight = -static_cast<LONG>(height);
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_RGB;
    header.biSizeImage = stride * height;

    void* dibBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(nullptr, reinterpret_cast<BITMAPINFO*>(&header), DIB_RGB_COLORS, &dibBits, nullptr, 0);
    if (!hBitmap || !dibBits) {
        if (hBitmap) {
            DeleteObject(hBitmap);
        }
        return false;
    }
    std::memcpy(dibBits, pixels.data(), header.biSizeImage);

    const SIZE_T dibSize = sizeof(BITMAPINFOHEADER) + header.biSizeImage;
    HGLOBAL hDib = GlobalAlloc(GHND | GMEM_SHARE, dibSize);
    if (!hDib) {
        DeleteObject(hBitmap);
        return false;
    }

    auto* dibData = static_cast<BYTE*>(GlobalLock(hDib));
    if (!dibData) {
        GlobalFree(hDib);
        DeleteObject(hBitmap);
        return false;
    }
    std::memcpy(dibData, &header, sizeof(BITMAPINFOHEADER));
    std::memcpy(dibData + sizeof(BITMAPINFOHEADER), pixels.data(), header.biSizeImage);
    GlobalUnlock(hDib);

    outBitmap = hBitmap;
    outDib = hDib;
    return true;
}

HICON LoadIconFromPng(const std::wstring& path, UINT targetSize) {
    UINT width = 0;
    UINT height = 0;
    UINT stride = 0;
    std::vector<BYTE> pixels;
    if (!LoadImagePixels(path, pixels, width, height, stride, targetSize)) {
        return nullptr;
    }

    BITMAPINFOHEADER header = {};
    header.biSize = sizeof(BITMAPINFOHEADER);
    header.biWidth = static_cast<LONG>(width);
    header.biHeight = -static_cast<LONG>(height);
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_RGB;
    header.biSizeImage = stride * height;

    BITMAPINFO info = {};
    info.bmiHeader = header;

    void* bits = nullptr;
    HBITMAP color = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!color || !bits) {
        if (color) {
            DeleteObject(color);
        }
        return nullptr;
    }
    std::memcpy(bits, pixels.data(), header.biSizeImage);

    HBITMAP mask = CreateBitmap(width, height, 1, 1, nullptr);

    ICONINFO iconInfo = {};
    iconInfo.fIcon = TRUE;
    iconInfo.hbmColor = color;
    iconInfo.hbmMask = mask;

    HICON icon = CreateIconIndirect(&iconInfo);

    if (mask) {
        DeleteObject(mask);
    }
    if (color) {
        DeleteObject(color);
    }
    return icon;
}

} // namespace

Application::Application(HINSTANCE instance)
    : instance_(instance),
      baseDirectory_(util::PathFromExecutable(L"")),
      configManager_(baseDirectory_) {
    config_.hotkey.primaryKey = VK_RSHIFT;
    config_.hotkey.requireWin = true;
    config_.hotkey.requireCtrl = false;
    config_.hotkey.requireAlt = false;
    config_.hotkey.requireShift = false;
    config_.hotkey.shiftMode = HotkeyConfig::ShiftMode::RightOnly;
    config_.paths.outputDirectory = L"output";
    config_.paths.sessionDirectory = L"temp\\sessions";
    config_.scrollsPerCapture = 3;
}

Application::~Application() {
    hotkeyManager_.Shutdown();
    if (uiFont_) {
        DeleteObject(uiFont_);
        uiFont_ = nullptr;
    }
    if (appIconLarge_) {
        DestroyIcon(appIconLarge_);
        appIconLarge_ = nullptr;
    }
    if (appIconSmall_) {
        DestroyIcon(appIconSmall_);
        appIconSmall_ = nullptr;
    }
    CoUninitialize();
}

bool Application::Initialize(int nCmdShow) {
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        return false;
    }

    configManager_.Load(config_);
    configManager_.Save(config_);
    EnsureDirectories();

    WNDCLASSW wc = {};
    wc.lpfnWndProc = Application::WindowProc;
    wc.hInstance = instance_;
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        0,
        kWindowClassName,
        L"Receipt Factor Auto Generator",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1100,
        800,
        nullptr,
        nullptr,
        instance_,
        this);
    if (!hwnd_) {
        return false;
    }

    ShowWindow(hwnd_, nCmdShow);
    UpdateWindow(hwnd_);

    if (!hotkeyManager_.Initialize(
            config_.hotkey,
            config_.scrollsPerCapture,
            [this]() { ToggleCaptureMode(); },
            [this]() { HandleCaptureRequest(); })) {
        MessageBoxW(hwnd_, L"Failed to initialize global hotkeys.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    webProcessor_.SetErrorCallback([this](const std::wstring& message) {
        HandleWebError(message);
    });

    return true;
}

int Application::Run() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK Application::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        auto create = reinterpret_cast<LPCREATESTRUCTW>(lParam);
        auto app = static_cast<Application*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->hwnd_ = hwnd;
        return TRUE;
    }

    auto app = reinterpret_cast<Application*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!app) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    switch (message) {
        case WM_CREATE:
            app->OnCreate();
            return 0;
        case WM_SIZE:
            app->OnSize();
            return 0;
        case WM_COMMAND:
            app->OnCommand(wParam);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

void Application::OnCreate() {
    HMENU menuBar = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    AppendMenuW(fileMenu, MF_STRING, kMenuOpenOutput, L"Open capture folder");
    AppendMenuW(fileMenu, MF_STRING, kMenuClearSessions, L"Clear session folder");
    AppendMenuW(fileMenu, MF_STRING, kMenuSettings, L"Hotkey settings");
    AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"Actions");
    SetMenu(hwnd_, menuBar);

    statusStatic_ = CreateWindowExW(0, L"STATIC", L"Ready.", WS_CHILD | WS_VISIBLE,
                                    16, 16, 600, 24, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStatusControlId)), instance_, nullptr);

    EnsureUIFont();
    if (uiFont_ && statusStatic_) {
        SendMessageW(statusStatic_, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
    }

    LoadWindowIcon();

    UpdateStatus(BuildIdleStatus());

    if (!webProcessor_.Initialize(hwnd_)) {
        MessageBoxW(hwnd_, L"Failed to initialize embedded browser.", L"Error", MB_ICONERROR | MB_OK);
    }

    OnSize();
}

void Application::OnSize() {
    if (!hwnd_) {
        return;
    }
    RECT client = {};
    GetClientRect(hwnd_, &client);
    const int padding = 16;
    const int statusHeight = 24;
    if (statusStatic_) {
        MoveWindow(statusStatic_, padding, padding, client.right - (padding * 2), statusHeight, TRUE);
    }
    RECT webRect = client;
    webRect.top = padding * 2 + statusHeight;
    if (webRect.top < webRect.bottom) {
        webProcessor_.Resize(webRect);
    }
}

void Application::OnCommand(WPARAM wParam) {
    switch (LOWORD(wParam)) {
        case kMenuOpenOutput:
            OpenOutputFolder();
            break;
        case kMenuClearSessions:
            ClearSessionDirectory();
            break;
        case kMenuSettings:
            ShowSettingsDialog();
            break;
        default:
            break;
    }
}

void Application::ToggleCaptureMode() {
    if (processingInFlight_) {
        return;
    }
    if (captureModeActive_) {
        ExitCaptureMode();
    } else {
        EnterCaptureMode();
    }
}

void Application::EnterCaptureMode() {
    if (captureModeActive_) {
        return;
    }
    HWND target = GetForegroundWindow();
    if (!target || target == hwnd_) {
        MessageBoxW(hwnd_, L"Please focus the game window before starting capture mode.", L"Notice", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!captureSession_.Begin(target, sessionDirectory_)) {
        MessageBoxW(hwnd_, L"Failed to begin capture session.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    currentCapturedFiles_.clear();
    captureModeActive_ = true;
    hotkeyManager_.SetCaptureMode(true);

    const size_t previousCount = currentCapturedFiles_.size();
    HandleCaptureRequest();

    std::wstring instruction = L"Scroll down ";
    instruction += std::to_wstring(config_.scrollsPerCapture);
    instruction += config_.scrollsPerCapture == 1 ? L" time per capture." : L" times per capture.";

    std::wstring status;
    if (currentCapturedFiles_.size() > previousCount) {
        status = L"Captured frame ";
        status += std::to_wstring(currentCapturedFiles_.size());
        status += L". ";
        status += instruction;
    } else {
        status = L"Capture mode ON. ";
        status += instruction;
    }
    UpdateStatus(status);
}

void Application::ExitCaptureMode() {
    if (!captureModeActive_) {
        return;
    }
    hotkeyManager_.SetCaptureMode(false);
    captureModeActive_ = false;

    auto captured = captureSession_.End();
    currentCapturedFiles_ = captured;
    if (captured.empty()) {
        UpdateStatus(L"Capture mode OFF. No frames captured.");
        return;
    }
    UpdateStatus(L"Preparing captures for clipboard...");
    StartProcessing(captured);
}

void Application::HandleCaptureRequest() {
    if (!captureModeActive_) {
        return;
    }
    const auto path = captureSession_.CaptureNext();
    if (!path.empty()) {
        currentCapturedFiles_.push_back(path);
        std::wstring text = L"Captured frame ";
        text += std::to_wstring(currentCapturedFiles_.size());
        UpdateStatus(text);
    }
}

void Application::StartProcessing(const std::vector<std::wstring>& capturedFiles) {
    if (capturedFiles.empty()) {
        return;
    }
    processingInFlight_ = true;
    std::vector<std::wstring> dataUrls;
    dataUrls.reserve(capturedFiles.size());

    for (const auto& file : capturedFiles) {
        std::ifstream fs(file, std::ios::binary);
        if (!fs) {
            continue;
        }
        std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(fs)), std::istreambuf_iterator<char>());
        if (buffer.empty()) {
            continue;
        }
        const std::wstring base64 = util::Base64FromBytes(buffer);
        if (base64.empty()) {
            continue;
        }
        dataUrls.push_back(L"data:image/png;base64," + base64);
    }

    if (!dataUrls.empty()) {
        webProcessor_.UpdateClipboardImages(dataUrls);
    } else {
        webProcessor_.UpdateClipboardImages({});
    }

    const bool clipboardOk = CopyImagesToClipboard(capturedFiles);
    processingInFlight_ = false;

    if (dataUrls.empty()) {
        UpdateStatus(L"No valid captures to prepare.");
        if (!clipboardOk) {
            MessageBoxW(hwnd_, L"Failed to copy capture to clipboard.", L"Clipboard error", MB_OK | MB_ICONERROR);
        }
        return;
    }

    std::wstring status;
    if (clipboardOk) {
        status = L"Images ready (";
        status += std::to_wstring(dataUrls.size());
        status += dataUrls.size() == 1 ? L" image). Use the page's clipboard import once." : L" images). Use the page's clipboard import once.";
    } else {
        status = L"Images ready for the embedded page, but clipboard copy failed.";
        MessageBoxW(hwnd_, status.c_str(), L"Clipboard error", MB_OK | MB_ICONERROR);
    }
    UpdateStatus(status);
}

void Application::HandleWebError(const std::wstring& message) {
    std::wstring text = L"Web error: " + message;
    UpdateStatus(text);
    MessageBoxW(hwnd_, text.c_str(), L"Web error", MB_OK | MB_ICONERROR);
}

void Application::UpdateStatus(const std::wstring& text) {
    if (statusStatic_) {
        SetWindowTextW(statusStatic_, text.c_str());
    }
}

void Application::OpenOutputFolder() {
    const std::wstring target = sessionDirectory_.empty() ? outputDirectory_ : sessionDirectory_;
    ShellExecuteW(hwnd_, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void Application::ShowSettingsDialog() {
    const auto updated = SettingsWindow::Show(instance_, hwnd_, config_.hotkey);
    if (!updated.has_value()) {
        return;
    }
    config_.hotkey = updated.value();
    hotkeyManager_.UpdateConfig(config_.hotkey);
    hotkeyManager_.SetScrollsPerCapture(config_.scrollsPerCapture);
    configManager_.Save(config_);
    if (!captureModeActive_ && !processingInFlight_) {
        UpdateStatus(BuildIdleStatus());
    }
}

void Application::EnsureUIFont() {
    if (uiFont_) {
        return;
    }
    HDC screen = GetDC(nullptr);
    const int dpi = screen ? GetDeviceCaps(screen, LOGPIXELSY) : 96;
    if (screen) {
        ReleaseDC(nullptr, screen);
    }
    const int logicalHeight = -MulDiv(10, dpi, 72);
    uiFont_ = CreateFontW(logicalHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (!uiFont_) {
        uiFont_ = CreateFontW(logicalHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"MS Shell Dlg 2");
    }
}

std::wstring Application::BuildIdleStatus() const {
    const std::wstring hotkeyText = hotkey::Describe(config_.hotkey);
    if (hotkeyText.empty()) {
        return L"Idle. Press the configured hotkey to start capturing.";
    }
    return L"Idle. Press " + hotkeyText + L" to start capturing.";
}

void Application::LoadWindowIcon() {
    const std::wstring iconPath = FindIconAsset();
    if (appIconLarge_) {
        DestroyIcon(appIconLarge_);
        appIconLarge_ = nullptr;
    }
    if (appIconSmall_) {
        DestroyIcon(appIconSmall_);
        appIconSmall_ = nullptr;
    }

    if (!iconPath.empty()) {
        appIconLarge_ = LoadIconFromPng(iconPath, 256);
        if (!appIconLarge_) {
            appIconLarge_ = LoadIconFromPng(iconPath, 0);
        }

        appIconSmall_ = LoadIconFromPng(iconPath, 32);
        if (!appIconSmall_) {
            appIconSmall_ = LoadIconFromPng(iconPath, 0);
        }
    }

    if (!appIconLarge_) {
        appIconLarge_ = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APPICON));
    }
    if (!appIconSmall_) {
        appIconSmall_ = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    }

    if (appIconLarge_) {
        SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(appIconLarge_));
        SetClassLongPtrW(hwnd_, GCLP_HICON, reinterpret_cast<LONG_PTR>(appIconLarge_));
    }
    if (appIconSmall_) {
        SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(appIconSmall_));
        SetClassLongPtrW(hwnd_, GCLP_HICONSM, reinterpret_cast<LONG_PTR>(appIconSmall_));
    }
}

std::wstring Application::FindIconAsset() const {
    std::vector<std::wstring> candidates = {
        util::JoinPath(baseDirectory_, L"appicon.png"),
        util::JoinPath(baseDirectory_, L"..\\appicon.png"),
        util::JoinPath(baseDirectory_, L"..\\resources\\appicon.png"),
        util::JoinPath(baseDirectory_, L"resources\\appicon.png"),
        L"C:\\Users\\yusir\\Documents\\receipt_kun_auto\\build\\appicon.png"
    };

    for (auto& candidate : candidates) {
        if (candidate.empty()) {
            continue;
        }
        std::error_code ec;
        std::filesystem::path path(candidate);
        if (std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec)) {
            return path.lexically_normal().wstring();
        }
    }
    return std::wstring();
}

void Application::ClearSessionDirectory() {
    if (captureModeActive_) {
        MessageBoxW(hwnd_, L"Stop capture mode before clearing sessions.", L"Capture active", MB_OK | MB_ICONWARNING);
        return;
    }
    if (processingInFlight_) {
        MessageBoxW(hwnd_, L"Wait until processing finishes before clearing sessions.", L"Processing", MB_OK | MB_ICONWARNING);
        return;
    }
    if (sessionDirectory_.empty()) {
        return;
    }

    const int choice = MessageBoxW(hwnd_, L"Delete all session captures? This cannot be undone.", L"Clear session folder", MB_ICONQUESTION | MB_YESNO);
    if (choice != IDYES) {
        return;
    }

    std::error_code ec;
    const std::filesystem::path sessionPath(sessionDirectory_);
    if (std::filesystem::exists(sessionPath, ec)) {
        std::filesystem::remove_all(sessionPath, ec);
        if (ec) {
            UpdateStatus(L"Failed to clear session folder.");
            MessageBoxW(hwnd_, L"Failed to clear the session folder.", L"Error", MB_OK | MB_ICONERROR);
            return;
        }
    }

    util::EnsureDirectory(sessionDirectory_);
    currentCapturedFiles_.clear();
    UpdateStatus(L"Session folder cleared.");
}

std::wstring Application::MakeAbsolutePath(const std::wstring& relative) const {
    std::filesystem::path p(relative);
    if (p.is_absolute()) {
        return p.wstring();
    }
    return util::JoinPath(baseDirectory_, relative);
}

void Application::EnsureDirectories() {
    outputDirectory_ = MakeAbsolutePath(config_.paths.outputDirectory);
    sessionDirectory_ = MakeAbsolutePath(config_.paths.sessionDirectory);
    util::EnsureDirectory(sessionDirectory_);
}

bool Application::CopyImagesToClipboard(const std::vector<std::wstring>& files) {
    if (files.empty()) {
        return false;
    }

    const std::wstring& target = files.back();
    HBITMAP hBitmap = nullptr;
    HGLOBAL hDib = nullptr;
    if (!CreateClipboardBitmaps(target, hBitmap, hDib)) {
        return false;
    }

    if (!OpenClipboard(hwnd_)) {
        DeleteObject(hBitmap);
        GlobalFree(hDib);
        return false;
    }

    if (!EmptyClipboard()) {
        CloseClipboard();
        DeleteObject(hBitmap);
        GlobalFree(hDib);
        return false;
    }

    bool anySuccess = false;

    if (SetClipboardData(CF_DIB, hDib)) {
        hDib = nullptr;
        anySuccess = true;
    }

    if (SetClipboardData(CF_BITMAP, hBitmap)) {
        hBitmap = nullptr;
        anySuccess = true;
    }

    CloseClipboard();

    if (hBitmap) {
        DeleteObject(hBitmap);
    }
    if (hDib) {
        GlobalFree(hDib);
    }

    return anySuccess;
}
