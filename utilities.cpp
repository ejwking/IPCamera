

#include "pch.h"
#include "utilities.h"
#include <fstream>
#include <vector>


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

// ##########################################################################################################

bool CConfigFile::load(const std::string& filename)
{
	m_params.clear();
	m_errors.clear();
	std::ifstream file(filename, std::ios::binary);
	if (!file.is_open()){
		m_errors.push_back("Cannot open file: " + filename);
		return false;
	}

	std::string line;
	int lineNumber = 0;
	while (std::getline(file, line)){
		++lineNumber;
		// Remove UTF‑8 BOM if present
		if (line.size() >= 3 &&
			(unsigned char)line[0] == 0xEF &&
			(unsigned char)line[1] == 0xBB &&
			(unsigned char)line[2] == 0xBF)
		{
			line = line.substr(3);
		}

		trim(line);
		if (line.empty())
			continue;
		if (line[0] == '#')
			continue;

		auto pos = line.find('=');
		if (pos == std::string::npos){
			m_errors.push_back("Malformed line " + std::to_string(lineNumber) + ": missing '='");
			continue;
		}

		std::string key   = line.substr(0, pos);
		std::string value = line.substr(pos + 1);
		trim(key);
		trim(value);
		if (key.empty()){
			m_errors.push_back("Malformed line " + std::to_string(lineNumber) + ": empty key");
			continue;
		}
		m_params[key] = value;
	}
	return m_errors.empty();
}

bool CConfigFile::hasKey(const std::string& key) const
{
	return m_params.find(key) != m_params.end();
}

std::string CConfigFile::getString(const std::string& key, const std::string& defaultValue) const
{
	auto it = m_params.find(key);
	return (it != m_params.end()) ? it->second : defaultValue;
}

int CConfigFile::getInt(const std::string& key, int defaultValue) const
{
	auto it = m_params.find(key);
	if (it == m_params.end())
		return defaultValue;

	try {
		return std::stoi(it->second);
	} catch (...) {
		return defaultValue;
	}
}

float CConfigFile::getFloat(const std::string& key, float defaultValue) const
{
	auto it = m_params.find(key);
	if (it == m_params.end())
		return defaultValue;

	try {
		return std::stof(it->second);
	} catch (...) {
		return defaultValue;
	}
}

double CConfigFile::getDouble(const std::string& key, double defaultValue) const
{
	auto it = m_params.find(key);
	if (it == m_params.end())
		return defaultValue;

	try {
		return std::stod(it->second);
	} catch (...) {
		return defaultValue;
	}
}

bool CConfigFile::getBool(const std::string& key, bool defaultValue) const
{
	auto it = m_params.find(key);
	if (it == m_params.end())
		return defaultValue;

	std::string v = it->second;
	toLower(v);
	if (v == "true" || v == "1" || v == "yes" || v == "on")
		return true;
	if (v == "false" || v == "0" || v == "no" || v == "off")
		return false;

	return defaultValue;
}

const std::vector<std::string>& CConfigFile::getErrors() const
{
	return m_errors;
}

void CConfigFile::trim(std::string& s)
{
	auto notSpace = [](int ch) { return !std::isspace(ch); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
	s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
}

void CConfigFile::toLower(std::string& s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
}

// ##########################################################################################################


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

/*
To check if a Win32 window (HWND) exists and belongs to a process that is still actively running, you must combine two checks: 
verifying the window handle validity and checking the execution status of its parent process.T
he most reliable approach requires using IsWindow alongside GetWindowThreadProcessId and OpenProcess.The Complete Verification Code (C++)

#include <windows.h>
#include <iostream>

bool IsWindowAliveAndActive(HWND hwnd) {
	// 1. Check if the window handle is still valid in the system
	if (!IsWindow(hwnd)) {
		return false;
	}

	// 2. Get the Process ID (PID) associated with the window
	DWORD processId = 0;
	GetWindowThreadProcessId(hwnd, &processId);
	if (processId == 0) {
		return false;
	}

	// 3. Open a handle to the process to check its exit status
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
	if (hProcess == NULL) {
		// If access is denied, the process likely still exists but has higher privileges
		// If the process was dead, ERROR_INVALID_PARAMETER would be thrown
		return (GetLastError() == ERROR_ACCESS_DENIED);
	}

	// 4. Verify if the process has exited
	DWORD exitCode = 0;
	if (GetExitCodeProcess(hProcess, &exitCode)) {
		CloseHandle(hProcess);
		// STILL_ACTIVE (259) means the process is still running
		return (exitCode == STILL_ACTIVE);
	}

	CloseHandle(hProcess);
	return false;
}

Why a Single Function is Not EnoughIsWindow(hwnd) is insufficient on its own: Window handles can be recycled by the operating system. 
If the original window closes and a new application creates a window, it might get assigned the exact same HWND value.
GetExitCodeProcess guards against recycling: By tracking the specific Process ID, you ensure that even if the window handle looks valid, you are checking whether the actual application that created it has closed.
PROCESS_QUERY_LIMITED_INFORMATION is preferred: Using this flag inside OpenProcess ensures your code works smoothly even when checking windows owned by elevated or administrative applications, reducing permission errors.
*/