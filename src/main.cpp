#include "divider/cli.h"
#include "divider/runner.h"

#include <exception>
#include <iostream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{

    void configureConsoleEncoding()
    {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif
    }

} // namespace

int main(int argc, char** argv)
{
    const std::string executableName = argc > 0 ? argv[0] : "divider";
    try
    {
        configureConsoleEncoding();

        const divider::Options options = divider::parseCommandLine(argc, argv);
        if (options.help)
        {
            std::cout << divider::usage(executableName);
            return 0;
        }

        divider::run(options, std::cout);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << "\n\n" << divider::usage(executableName);
        return 1;
    }
}
