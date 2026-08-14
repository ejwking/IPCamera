

#include "pch.h"
#include "utilities.h"


CString Utf8(const std::string& s)
{
	return CString(CA2W(s.c_str(), CP_UTF8));
}

// Convert UTF-16 (std::wstring) to UTF-8 (std::string)
std::string Utf16ToUtf8(const std::wstring& utf16Str)
{
	if (utf16Str.empty()) return "";

	int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &utf16Str[0], (int)utf16Str.size(), NULL, 0, NULL, NULL);
	std::string utf8Str(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, &utf16Str[0], (int)utf16Str.size(), &utf8Str[0], sizeNeeded, NULL, NULL);

	return utf8Str;
}




// https://algomaster.io/learn/concurrency-interview/cpp-creating-threads

// C++ provides limited thread configuration through the standard library. Many properties like names and priorities require platform-specific APIs.
// Good thread names are invaluable for debugging, so production code typically includes this platform-specific logic.
// 
// Thread Naming (Platform-Specific)
// The C++ standard doesn't provide thread naming. You must use native handles:

/*
#include <thread>

#ifdef __linux__
#include <pthread.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

void set_thread_name(std::thread& t, const char* name) {
#ifdef __linux__
    pthread_setname_np(t.native_handle(), name);
#elif defined(__APPLE__)
    // macOS: can only set name from within the thread
    // pthread_setname_np(name);
#elif defined(_WIN32)
    // Windows 10 1607+: SetThreadDescription
    // SetThreadDescription(t.native_handle(), L"name");
#endif
}

// Alternative: set name from within the thread
void worker() {
#ifdef __linux__
    pthread_setname_np(pthread_self(), "Worker");
#elif defined(__APPLE__)
    pthread_setname_np("Worker");
#endif
    // ... work
}

*/