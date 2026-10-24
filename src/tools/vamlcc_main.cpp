// S280 (dep:S255) — vamlcc CLI entry point. A thin argv → seam → exit-code
// shell over the vaml-tools compiler (vaml::tools::run_vamlcc — the headless
// seam in Vamlc.h/Vamlc.cpp). The real compiler logic lives in vaml-tools so
// it is testable by vaml-tools-tests; this file is the only executable shell.
//
// Exit codes (see Vamlc.h): 0 success, 1 usage, 2 I/O failure, 3 compile failure.
//
// S287 wires this executable into CMake custom commands (per .vaml → .vbaml);
// until then it builds + runs as a standalone tool.

#include "vaml/tools/Vamlc.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

// The usage line printed to stderr on a bad invocation.
void print_usage()
{
    std::cerr << "usage: vamlcc <in.vaml> <out.vbaml>\n";
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i)
    {
        args.emplace_back(argv[i]);
    }

    const int code = vaml::tools::run_vamlcc(args);
    if (code != 0)
    {
        print_usage();
    }
    return code;
}
