﻿#include <vector>
#include <string>

#include <utf8.h>
#include <fmt/printf.h>

int usagi_main(const std::vector<std::string> &args);

namespace usagi
{
std::vector<std::string> argsFromMain(int argc, wchar_t *argv[])
{
    std::vector<std::string> args;
    args.reserve(argc - 1);
    for(auto i = 1; i < argc; ++i)
    {
        std::wstring_view u16_arg = argv[i];
        std::string u8_arg;
        utf8::utf16to8(
            u16_arg.begin(), u16_arg.end(),
            std::back_inserter(u8_arg)
        );
        args.push_back(std::move(u8_arg));
    }
    return args;
}

std::vector<std::string> argsFromMain(int argc, char *argv[])
{
    std::vector<std::string> args;
    args.reserve(argc - 1);
    for(auto i = 1; i < argc; ++i)
        args.push_back(argv[i]);
    return args;
}

int wrappedMain(const std::vector<std::string> &args)
{
    try
    {
        return usagi_main(args);
    }
    catch(const std::exception &e)
    {
        fmt::print("{}\n", e.what());
    }
    catch(...)
    {
        fmt::print("Unknown unhandled exception occurred.\n");
    }
    return 1;
}
}
