#include "Unicode.hpp"

#include <string>

#include <utf8.h>

std::wstring usagi::utf8To16(const std::string &string)
{
    std::wstring utf16line;
    utf8::utf8to16(string.begin(), string.end(),
        back_inserter(utf16line));
    return utf16line;
}

std::string usagi::utf16To8(const std::wstring &string)
{
    std::string utf8line;
    utf8::utf16to8(string.begin(), string.end(), back_inserter(utf8line));
    return utf8line;
}
