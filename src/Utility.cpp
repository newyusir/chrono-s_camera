#include "Utility.h"

#include <algorithm>
#include <chrono>
#include <codecvt>
#include <filesystem>
#include <locale>
#include <cwctype>
#include <wincrypt.h>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Crypt32.lib")

namespace util {

std::wstring Trim(const std::wstring& input) {
    const auto begin = input.find_first_not_of(L" \t\r\n");
    if (begin == std::wstring::npos) {
        return L"";
    }
    const auto end = input.find_last_not_of(L" \t\r\n");
    return input.substr(begin, end - begin + 1);
}

std::wstring ToLower(const std::wstring& input) {
    std::wstring result = input;
    std::transform(result.begin(), result.end(), result.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(towlower(c));
    });
    return result;
}

std::wstring JoinPath(const std::wstring& base, const std::wstring& child) {
    std::filesystem::path p(base);
    p /= child;
    return p.wstring();
}

bool EnsureDirectory(const std::wstring& path) {
    std::error_code ec;
    std::filesystem::path p(path);
    if (std::filesystem::exists(p, ec)) {
        return std::filesystem::is_directory(p, ec);
    }
    return std::filesystem::create_directories(p, ec);
}

std::wstring TimestampString() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto tt = system_clock::to_time_t(now);
    std::tm tm_snapshot;
    localtime_s(&tm_snapshot, &tt);
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;

    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"%04d%02d%02d_%02d%02d%02d_%03lld",
        tm_snapshot.tm_year + 1900,
        tm_snapshot.tm_mon + 1,
        tm_snapshot.tm_mday,
        tm_snapshot.tm_hour,
        tm_snapshot.tm_min,
        tm_snapshot.tm_sec,
        static_cast<long long>(ms));
    return buffer;
}

std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) {
        return L"";
    }
    int required = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
    if (required <= 0) {
        return L"";
    }
    std::wstring result(static_cast<size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), result.data(), required);
    return result;
}

std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) {
        return std::string();
    }
    int required = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return std::string();
    }
    std::string result(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), result.data(), required, nullptr, nullptr);
    return result;
}

std::wstring PathFromExecutable(const std::wstring& relative) {
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::filesystem::path exePath(buffer, buffer + length);
    exePath = exePath.parent_path();
    exePath /= relative;
    return exePath.wstring();
}

std::vector<std::wstring> Split(const std::wstring& input, wchar_t delimiter) {
    std::vector<std::wstring> tokens;
    std::wstring current;
    for (auto ch : input) {
        if (ch == delimiter) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

std::wstring Base64FromBytes(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return L"";
    }
    DWORD required = 0;
    if (!CryptBinaryToStringW(data.data(), static_cast<DWORD>(data.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &required)) {
        return L"";
    }
    std::wstring result(required, L'\0');
    if (!CryptBinaryToStringW(data.data(), static_cast<DWORD>(data.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, result.data(), &required)) {
        return L"";
    }
    if (!result.empty() && result.back() == L'\0') {
        result.pop_back();
    }
    return result;
}

std::vector<uint8_t> Base64ToBytes(const std::wstring& base64) {
    if (base64.empty()) {
        return {};
    }
    DWORD required = 0;
    if (!CryptStringToBinaryW(base64.c_str(), static_cast<DWORD>(base64.size()), CRYPT_STRING_BASE64, nullptr, &required, nullptr, nullptr)) {
        return {};
    }
    std::vector<uint8_t> buffer(required);
    if (!CryptStringToBinaryW(base64.c_str(), static_cast<DWORD>(base64.size()), CRYPT_STRING_BASE64, buffer.data(), &required, nullptr, nullptr)) {
        return {};
    }
    buffer.resize(required);
    return buffer;
}

} // namespace util
