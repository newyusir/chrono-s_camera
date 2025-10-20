#pragma once

#include <string>
#include <vector>
#include <windows.h>

namespace util {

std::wstring Trim(const std::wstring& input);
std::wstring ToLower(const std::wstring& input);
std::wstring JoinPath(const std::wstring& base, const std::wstring& child);
bool EnsureDirectory(const std::wstring& path);
std::wstring TimestampString();
std::wstring Utf8ToWide(const std::string& str);
std::string WideToUtf8(const std::wstring& wstr);
std::wstring PathFromExecutable(const std::wstring& relative);
std::vector<std::wstring> Split(const std::wstring& input, wchar_t delimiter);
std::wstring Base64FromBytes(const std::vector<uint8_t>& data);
std::vector<uint8_t> Base64ToBytes(const std::wstring& base64);

} // namespace util
