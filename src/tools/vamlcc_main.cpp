// S280 (dep:S255) — vamlcc CLI entry point. A thin argv → seam → exit-code
// shell over the vaml-tools compiler (vaml::tools::run_vamlcc — the headless
// seam in Vamlc.h/Vamlc.cpp). The real compiler logic lives in vaml-tools so
// it is testable by vaml-tools-tests; this file is the only executable shell.
//
// Modes:
//   vamlcc <in.vaml> <out.vbaml>          — compile (default; S280)
//   vamlcc --embed <in.vbaml> <out.h> <out.cpp> [symbol]  — embed (S287)
//
// Exit codes (see Vamlc.h): 0 success, 1 usage, 2 I/O failure, 3 compile failure.
//
// S287 wires this executable into CMake custom commands (per .vaml → .vbaml
// sidecar + the embedded twin via --embed); until then it builds + runs as a
// standalone tool.

#include "vaml/tools/Vamlc.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

// The usage line printed to stderr on a bad invocation.
void print_usage()
{
    std::cerr << "usage:\n"
              << "  vamlcc <in.vaml> <out.vbaml>\n"
              << "  vamlcc --embed <in.vbaml> <out.h> <out.cpp> [symbol]\n";
}

// Reads the whole file at `path` into `bytes`. Returns false on I/O failure.
bool read_file_bytes(const std::string& path, std::vector<std::uint8_t>& bytes)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    in.seekg(0, std::ios::end);
    const std::streamsize sz = static_cast<std::streamsize>(in.tellg());
    in.seekg(0, std::ios::beg);
    bytes.assign(static_cast<std::size_t>(sz), 0);
    if (sz > 0 && !in.read(reinterpret_cast<char*>(bytes.data()), sz))
    {
        return false;
    }
    return true;
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

    // S287 — embed mode: vamlcc --embed <in.vbaml> <out.h> <out.cpp> [symbol].
    if (args.size() >= 2 && args[1] == "--embed")
    {
        if (args.size() < 5 || args.size() > 6)
        {
            print_usage();
            return 1;
        }
        const std::string& inVbaml = args[2];
        const std::string& outH = args[3];
        const std::string& outCpp = args[4];
        const std::string symbol = args.size() == 6 ? args[5] : std::string();

        std::vector<std::uint8_t> bytes;
        if (!read_file_bytes(inVbaml, bytes))
        {
            std::cerr << "error: cannot read '" << inVbaml << "'\n";
            return 2;
        }
        if (!vaml::tools::write_embed(outH, outCpp, symbol, bytes))
        {
            std::cerr << "error: cannot write embed outputs\n";
            return 2;
        }
        return 0;
    }

    // Default compile mode.
    const int code = vaml::tools::run_vamlcc(args);
    if (code != 0)
    {
        print_usage();
    }
    return code;
}
