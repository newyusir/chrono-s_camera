#pragma once

#include <string>

#include "ConfigManager.h"

namespace hotkey {

std::wstring Describe(const HotkeyConfig& config);
std::wstring DescribeKey(UINT vk);

} // namespace hotkey
