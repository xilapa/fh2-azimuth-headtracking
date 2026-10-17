#pragma once
#include <windows.h>
#include <string>

namespace fh2 {

// Writes the commented default INI if it does not exist, first moving an INI
// from an older config version aside to <name>.old.bak.
void ensureConfigFile(const std::wstring& iniPath);

} // namespace fh2
