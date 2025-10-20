#pragma once

#include <functional>
#include <string>
#include <vector>

#include <wrl/client.h>
#include <windows.h>
#include <webview2.h>

class WebProcessor {
public:
    using ErrorCallback = std::function<void(const std::wstring& message)>;

    WebProcessor();
    ~WebProcessor();

    bool Initialize(HWND parentWindow);
    void Resize(const RECT& bounds);
    void SetErrorCallback(ErrorCallback cb) { errorCallback_ = std::move(cb); }
    void UpdateClipboardImages(const std::vector<std::wstring>& dataUrls);

private:
    void SetupEventHandlers();
    void HandleWebMessage(const std::wstring& message);
    void PostStringMessage(const std::wstring& message) const;
    void SendClipboardResponse(const std::wstring& requestId) const;
    void NotifyClipboardInventory() const;

    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webView_;

    HWND parentWindow_ = nullptr;
    RECT pendingBounds_{};
    bool hasPendingBounds_ = false;

    bool bridgeReady_ = false;

    std::vector<std::wstring> clipboardImages_;
    ErrorCallback errorCallback_{};
};
