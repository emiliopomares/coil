#include <coil/foundation/assert.hpp>

#include <array>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string_view>
#include <system_error>

namespace coil::foundation::detail {

namespace {

void writeToStderr(std::string_view text) noexcept {
    // Nothing can be done if stderr fails; the abort is what matters.
    static_cast<void>(std::fwrite(text.data(), 1, text.size(), stderr));
}

} // namespace

void expectsFailed(const char* condition, std::source_location where) noexcept {
    std::array<char, 16> line{};
    const auto [end, error] = std::to_chars(line.begin(), line.end(), where.line());
    writeToStderr(where.file_name());
    writeToStderr(":");
    writeToStderr(error == std::errc{} ? std::string_view{line.begin(), end} : "?");
    writeToStderr(": precondition failed: ");
    writeToStderr(condition);
    writeToStderr("\n");
    std::abort();
}

} // namespace coil::foundation::detail
