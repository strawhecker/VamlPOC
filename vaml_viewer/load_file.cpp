// vaml_viewer — load_file seam implementation (S221, dep:S220).
//
// See load_file.h for the contract. This is the single source of truth for the
// viewer's file→tree entry path, compiled by both the Windows/Linux apps and
// the headless test target.
#include "load_file.h"

#include "vaml/core/DependencyObject.h"
#include "vaml/core/ElementResolver.h"  // ElementResolutionException
#include "vaml/core/ParsedElement.h"    // XmlParseException (+ XmlScanException via XmlScanner.h)
#include "vaml/core/ValueConversionError.h" // ValueConversionException
#include "vaml/core/VamlReader.h"        // VamlReader + VamlReaderException

#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace vaml {

namespace {

// Plain C++17 std::filesystem is not available under the strictest old-style
// flag sets this project's app builds may impose; here existing() uses the
// std::ifstream open probe (RAII, no external dependency) so we can honestly
// distinguish "NoSuchFile" (the path does not exist) from "ReadError" (the
// path exists but could not be opened/read — e.g. a permission or mid-read
// failure). Both land as categorized, never-silent errors at the CLI boundary.
enum class ReadStatus
{
    Missing,   // the path does not exist
    NoRead,    // the path exists but could not be opened/fully read
    ReadOk,    // fully read into `out`
};

ReadStatus read_file(const std::string& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
    {
        // Open failed: decide Missing vs NoRead by probing existence with a
        // second (read) open attempt. A path that also fails to open on probe
        // is treated as Missing — the operator's actionable category.
        std::ifstream probe(path, std::ios::binary);
        return probe.is_open() ? ReadStatus::NoRead : ReadStatus::Missing;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    if (in.bad())
    {
        return ReadStatus::NoRead;
    }
    out = ss.str();
    return ReadStatus::ReadOk;
}

// Converts VamlReader's structured exception taxonomy into a typed LoadResult.
// Every document-level failure the reader can produce is mapped explicitly —
// the S085/S088/S089/S091/S094 structured errors all carry their human message
// into `detail` (never silent). Any other std::exception that escapes Load is
// still surfaced as a categorized ParseError with its what(), so the entry
// path can never silently swallow a failure.
LoadResult failure_for(std::exception_ptr ex)
{
    try
    {
        std::rethrow_exception(std::move(ex));
    }
    catch (const XmlScanException& e)
    {
        return {false, nullptr, FileLoadError::ParseError, e.what()};
    }
    catch (const XmlParseException& e)
    {
        return {false, nullptr, FileLoadError::ParseError, e.what()};
    }
    catch (const VamlReaderException& e)
    {
        return {false, nullptr, FileLoadError::ParseError, e.what()};
    }
    catch (const ElementResolutionException& e)
    {
        return {false, nullptr, FileLoadError::ParseError, e.what()};
    }
    catch (const ValueConversionException& e)
    {
        return {false, nullptr, FileLoadError::ParseError, e.what()};
    }
    catch (const std::exception& e)
    {
        return {false, nullptr, FileLoadError::ParseError, e.what()};
    }
}

} // namespace

LoadResult load_text(const std::string& text)
{
    try
    {
        VamlReader reader;
        std::unique_ptr<DependencyObject> root = reader.Load(text);
        return {true, std::move(root), FileLoadError::None, {}};
    }
    catch (...)
    {
        return failure_for(std::current_exception());
    }
}

LoadResult load_file(const std::string& path)
{
    std::string contents;
    switch (read_file(path, contents))
    {
    case ReadStatus::Missing:
        return {false, nullptr, FileLoadError::NoSuchFile,
                "no such file: '" + path + "'"};
    case ReadStatus::NoRead:
        return {false, nullptr, FileLoadError::ReadError,
                "cannot read file: '" + path + "'"};
    case ReadStatus::ReadOk:
        break; // fall through to the parse path
    }
    LoadResult result = load_text(contents);
    return result;
}

} // namespace vaml
