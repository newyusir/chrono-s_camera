#include "WebProcessor.h"

#include <sstream>

#include <combaseapi.h>
#include <wrl.h>
#include <wrl/event.h>

namespace {

std::wstring DecodeJsonString(const std::wstring& json) {
    size_t start = 0;
    size_t end = json.size();
    if (end >= 2 && json.front() == L'"' && json.back() == L'"') {
        start = 1;
        end -= 1;
    }
    std::wstring out;
    out.reserve(end - start);
    for (size_t i = start; i < end; ++i) {
        wchar_t ch = json[i];
        if (ch == L'\\' && i + 1 < end) {
            wchar_t next = json[++i];
            switch (next) {
                case L'"': out.push_back(L'"'); break;
                case L'\\': out.push_back(L'\\'); break;
                case L'/': out.push_back(L'/'); break;
                case L'b': out.push_back(L'\b'); break;
                case L'f': out.push_back(L'\f'); break;
                case L'n': out.push_back(L'\n'); break;
                case L'r': out.push_back(L'\r'); break;
                case L't': out.push_back(L'\t'); break;
                case L'u': {
                    if (i + 4 < end) {
                        unsigned value = 0;
                        bool valid = true;
                        for (int j = 0; j < 4; ++j) {
                            wchar_t hex = json[i + 1 + j];
                            value <<= 4;
                            if (hex >= L'0' && hex <= L'9') {
                                value |= static_cast<unsigned>(hex - L'0');
                            } else if (hex >= L'a' && hex <= L'f') {
                                value |= static_cast<unsigned>(hex - L'a' + 10);
                            } else if (hex >= L'A' && hex <= L'F') {
                                value |= static_cast<unsigned>(hex - L'A' + 10);
                            } else {
                                valid = false;
                                break;
                            }
                        }
                        if (valid) {
                            out.push_back(static_cast<wchar_t>(value));
                            i += 4;
                        }
                    }
                    break;
                }
                default:
                    out.push_back(next);
                    break;
            }
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

} // namespace

WebProcessor::WebProcessor() = default;
WebProcessor::~WebProcessor() = default;

bool WebProcessor::Initialize(HWND parentWindow) {
    parentWindow_ = parentWindow;
    bridgeReady_ = false;

    auto environmentHandler = Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [this](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
            if (FAILED(result)) {
                if (errorCallback_) {
                    std::wstringstream ss;
                    ss << L"WebView2 environment creation failed (0x" << std::hex << result << L")";
                    errorCallback_(ss.str());
                }
                return result;
            }
            environment_ = environment;

            auto controllerHandler = Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                    if (FAILED(result)) {
                        if (errorCallback_) {
                            std::wstringstream ss;
                            ss << L"WebView2 controller creation failed (0x" << std::hex << result << L")";
                            errorCallback_(ss.str());
                        }
                        return result;
                    }
                    controller_ = controller;
                    controller_->get_CoreWebView2(&webView_);
                    if (hasPendingBounds_) {
                        controller_->put_Bounds(pendingBounds_);
                    } else {
                        RECT bounds = {};
                        GetClientRect(parentWindow_, &bounds);
                        controller_->put_Bounds(bounds);
                    }

                    SetupEventHandlers();

                    const std::wstring script = LR"JS((() => {
                        const pending = new Map();
                        let requestId = 0;
                        const requestTimeout = 4000;

                        const ensureBannerElement = () => {
                            let element = document.getElementById('nativeClipboardStatus');
                            if (!element) {
                                element = document.createElement('div');
                                element.id = 'nativeClipboardStatus';
                                element.style.position = 'fixed';
                                element.style.bottom = '16px';
                                element.style.right = '16px';
                                element.style.padding = '8px 12px';
                                element.style.background = 'rgba(30, 32, 36, 0.85)';
                                element.style.color = '#fff';
                                element.style.fontSize = '14px';
                                element.style.borderRadius = '4px';
                                element.style.zIndex = '9999';
                                element.style.pointerEvents = 'none';
                                element.style.boxShadow = '0 2px 6px rgba(0,0,0,0.25)';
                                element.textContent = 'Captured images not ready.';
                                document.body.appendChild(element);
                            }
                            return element;
                        };

                        const updateBanner = (count) => {
                            const element = ensureBannerElement();
                            if (count && count > 0) {
                                element.textContent = 'Captured images ready: ' + count;
                                element.style.opacity = '1';
                            } else {
                                element.textContent = 'Captured images not ready.';
                                element.style.opacity = '0.6';
                            }
                        };

                        const requestNativeClipboard = () => new Promise((resolve) => {
                            const id = String(++requestId);
                            pending.set(id, resolve);
                            window.chrome.webview.postMessage('clipboardRequest|' + id);
                            setTimeout(() => {
                                if (pending.has(id)) {
                                    pending.delete(id);
                                    resolve([]);
                                }
                            }, requestTimeout);
                        });

                        window.chrome.webview.addEventListener('message', async (event) => {
                            const data = String(event.data);
                            if (data.startsWith('clipboardResponse|')) {
                                const parts = data.split('|');
                                const id = parts[1] || '';
                                const resolver = pending.get(id);
                                if (!resolver) {
                                    return;
                                }
                                pending.delete(id);
                                const count = parseInt(parts[2] || '0', 10);
                                const items = [];
                                for (let i = 0; i < count; ++i) {
                                    const dataUrl = parts[3 + i];
                                    if (!dataUrl) {
                                        continue;
                                    }
                                    const item = {
                                        types: ['image/png'],
                                        getType: async (type) => {
                                            if (type !== 'image/png') {
                                                throw new Error('Unsupported type');
                                            }
                                            const response = await fetch(dataUrl);
                                            return await response.blob();
                                        }
                                    };
                                    items.push(item);
                                }
                                resolver(items);
                            } else if (data.startsWith('clipboardInventory|')) {
                                const count = parseInt(data.split('|')[1] || '0', 10);
                                updateBanner(count);
                            }
                        });

                        const originalRead = navigator.clipboard && navigator.clipboard.read ? navigator.clipboard.read.bind(navigator.clipboard) : null;

                        if (navigator.clipboard) {
                            navigator.clipboard.read = async () => {
                                const nativeItems = await requestNativeClipboard();
                                if (nativeItems && nativeItems.length) {
                                    return nativeItems;
                                }
                                if (originalRead) {
                                    return originalRead();
                                }
                                return [];
                            };
                        }

                        updateBanner(0);
                        window.chrome.webview.postMessage('bridgeReady');
                    })();)JS";

                    webView_->AddScriptToExecuteOnDocumentCreated(script.c_str(), nullptr);
                    webView_->Navigate(L"https://lt900ed.github.io/receipt_factor/");
                    return S_OK;
                });

            environment_->CreateCoreWebView2Controller(parentWindow_, controllerHandler.Get());
            return S_OK;
        });

    const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, environmentHandler.Get());
    if (FAILED(hr)) {
        if (errorCallback_) {
            std::wstringstream ss;
            ss << L"CreateCoreWebView2EnvironmentWithOptions failed (0x" << std::hex << hr << L")";
            errorCallback_(ss.str());
        }
        return false;
    }
    return true;
}

void WebProcessor::Resize(const RECT& bounds) {
    pendingBounds_ = bounds;
    hasPendingBounds_ = true;
    if (controller_) {
        controller_->put_Bounds(bounds);
    }
}

void WebProcessor::UpdateClipboardImages(const std::vector<std::wstring>& dataUrls) {
    clipboardImages_ = dataUrls;
    NotifyClipboardInventory();
}

void WebProcessor::SetupEventHandlers() {
    if (!webView_) {
        return;
    }

    webView_->add_WebMessageReceived(Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
        [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
            LPWSTR json = nullptr;
            if (SUCCEEDED(args->get_WebMessageAsJson(&json)) && json) {
                std::wstring decoded = DecodeJsonString(json);
                CoTaskMemFree(json);
                HandleWebMessage(decoded);
            }
            return S_OK;
        }).Get(), nullptr);
}

void WebProcessor::HandleWebMessage(const std::wstring& message) {
    const auto separator = message.find(L'|');
    const std::wstring type = separator == std::wstring::npos ? message : message.substr(0, separator);
    const std::wstring payload = separator == std::wstring::npos ? std::wstring() : message.substr(separator + 1);

    if (type == L"clipboardRequest") {
        SendClipboardResponse(payload);
    } else if (type == L"bridgeReady") {
        bridgeReady_ = true;
        NotifyClipboardInventory();
    }
}

void WebProcessor::PostStringMessage(const std::wstring& message) const {
    if (webView_) {
        webView_->PostWebMessageAsString(message.c_str());
    }
}

void WebProcessor::SendClipboardResponse(const std::wstring& requestId) const {
    std::wstringstream ss;
    ss << L"clipboardResponse|" << requestId << L"|" << static_cast<unsigned long long>(clipboardImages_.size());
    for (const auto& dataUrl : clipboardImages_) {
        ss << L"|" << dataUrl;
    }
    PostStringMessage(ss.str());
}

void WebProcessor::NotifyClipboardInventory() const {
    if (!bridgeReady_ || !webView_) {
        return;
    }
    std::wstringstream ss;
    ss << L"clipboardInventory|" << static_cast<unsigned long long>(clipboardImages_.size());
    PostStringMessage(ss.str());
}
