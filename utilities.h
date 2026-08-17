
#pragma once

#include <string>

#define num_entries(name) (sizeof(name)/sizeof((name)[0]))

extern CString Utf8(const std::string& s);
extern std::string Utf16ToUtf8(const std::wstring& utf16Str);