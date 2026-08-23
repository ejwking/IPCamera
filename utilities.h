
#pragma once

#include <string>
#include <unordered_map>
#include <algorithm>

#define num_entries(name) (sizeof(name)/sizeof((name)[0]))

extern CString Utf8(const std::string& s);
extern std::string Utf16ToUtf8(const std::wstring& utf16Str);

class CConfigFile
{
public:
    bool load(const std::string& filename);
    bool hasKey(const std::string& key) const;

    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    int getInt(const std::string& key, int defaultValue = 0) const;
    float getFloat(const std::string& key, float defaultValue = 0.0f) const;
    double getDouble(const std::string& key, double defaultValue = 0.0) const;
    bool getBool(const std::string& key, bool defaultValue = false) const;
    const std::vector<std::string>& getErrors() const;

private:
    static void trim(std::string& s);
    static void toLower(std::string& s);
    std::unordered_map<std::string, std::string> m_params;
    std::vector<std::string> m_errors;
};
