#include "CaptureSession.h"

#include "Utility.h"

#include <filesystem>
#include <memory>

#include <wrl/client.h>
#include <wincodec.h>

#pragma comment(lib, "Windowscodecs.lib")

namespace {
Microsoft::WRL::ComPtr<IWICImagingFactory> g_wicFactory;

bool EnsureWicFactory() {
    if (!g_wicFactory) {
        auto hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_wicFactory));
        if (FAILED(hr)) {
            return false;
        }
    }
    return true;
}

bool SaveBitmapToPng(HBITMAP bitmap, const std::wstring& path) {
    if (!EnsureWicFactory()) {
        return false;
    }
    BITMAP bmp = {};
    if (!GetObjectW(bitmap, sizeof(BITMAP), &bmp)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmap> wicBitmap;
    auto hr = g_wicFactory->CreateBitmapFromHBITMAP(bitmap, nullptr, WICBitmapUsePremultipliedAlpha, &wicBitmap);
    if (FAILED(hr)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICStream> stream;
    hr = g_wicFactory->CreateStream(&stream);
    if (FAILED(hr)) {
        return false;
    }
    hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
    hr = g_wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (FAILED(hr)) {
        return false;
    }
    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
    Microsoft::WRL::ComPtr<IPropertyBag2> props;
    hr = encoder->CreateNewFrame(&frame, &props);
    if (FAILED(hr)) {
        return false;
    }
    hr = frame->Initialize(nullptr);
    if (FAILED(hr)) {
        return false;
    }

    hr = frame->SetSize(static_cast<UINT>(bmp.bmWidth), static_cast<UINT>(bmp.bmHeight));
    if (FAILED(hr)) {
        return false;
    }

    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    hr = frame->SetPixelFormat(&format);
    if (FAILED(hr)) {
        return false;
    }

    hr = frame->WriteSource(wicBitmap.Get(), nullptr);
    if (FAILED(hr)) {
        return false;
    }

    hr = frame->Commit();
    if (FAILED(hr)) {
        return false;
    }
    hr = encoder->Commit();
    if (FAILED(hr)) {
        return false;
    }
    return true;
}

} // namespace

CaptureSession::CaptureSession() = default;

bool CaptureSession::Begin(HWND targetWindow, const std::wstring& baseDirectory) {
    if (active_) {
        return false;
    }
    targetWindow_ = targetWindow;
    baseDirectory_ = baseDirectory;
    captureIndex_ = 0;
    capturedFiles_.clear();

    if (!IsWindow(targetWindow_)) {
        return false;
    }

    if (!util::EnsureDirectory(baseDirectory_)) {
        return false;
    }

    sessionRoot_ = util::JoinPath(baseDirectory_, util::TimestampString());
    rawDirectory_ = util::JoinPath(sessionRoot_, L"raw");

    if (!util::EnsureDirectory(sessionRoot_) || !util::EnsureDirectory(rawDirectory_)) {
        return false;
    }

    active_ = true;
    return true;
}

std::wstring CaptureSession::CaptureNext() {
    if (!active_ || !IsWindow(targetWindow_)) {
        return std::wstring();
    }

    const auto fileName = NextCaptureFilename();
    const auto fullPath = util::JoinPath(rawDirectory_, fileName);
    if (!CaptureWindowToFile(targetWindow_, fullPath)) {
        return std::wstring();
    }
    capturedFiles_.push_back(fullPath);
    ++captureIndex_;
    return fullPath;
}

std::vector<std::wstring> CaptureSession::End() {
    active_ = false;
    targetWindow_ = nullptr;
    return capturedFiles_;
}

std::wstring CaptureSession::NextCaptureFilename() const {
    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"shot_%04zu.png", captureIndex_ + 1);
    return buffer;
}

bool CaptureSession::CaptureWindowToFile(HWND hwnd, const std::wstring& filePath) {
    RECT rect = {};
    if (!GetWindowRect(hwnd, &rect)) {
        return false;
    }
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        return false;
    }

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    HGDIOBJ oldObj = SelectObject(hdcMem, hBitmap);

    BOOL printed = PrintWindow(hwnd, hdcMem, PW_RENDERFULLCONTENT);
    if (!printed) {
        HDC hdcWindow = GetWindowDC(hwnd);
        BitBlt(hdcMem, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY | CAPTUREBLT);
        ReleaseDC(hwnd, hdcWindow);
    }

    SelectObject(hdcMem, oldObj);

    bool saved = SaveBitmapToPng(hBitmap, filePath);

    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    return saved;
}
