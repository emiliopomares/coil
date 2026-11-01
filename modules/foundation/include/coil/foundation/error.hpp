#pragma once

#include <cstdint>
#include <expected>
#include <format>
#include <string>
#include <string_view>

namespace coil::foundation {

/// Project-wide error codes. Numeric values are stable and never reused: they may cross process
/// boundaries (remote protocol, MCP) and be persisted.
// NOLINTNEXTLINE(performance-enum-size): wire-format headroom; codes are never reused (ADR-0013).
enum class ErrorCode : std::uint16_t {
    InvalidArgument = 1, ///< A caller-supplied value is outside the accepted domain.
    NotFound = 2,        ///< A referenced entity (id, handle, name) does not exist.
    Unavailable = 3,     ///< A required resource or service cannot be used right now.
    Internal = 4,        ///< An unexpected failure inside Coil or a backend.
};

/// Returns the enumerator name ("InvalidArgument", …), or "Unknown" for any other value.
[[nodiscard]] std::string_view toString(ErrorCode code) noexcept;

/// A recoverable error: a code for programs to branch on, plus an optional message for humans.
/// Program logic must never branch on the message.
class Error {
public:
    explicit Error(ErrorCode code, std::string message = {});

    [[nodiscard]] ErrorCode code() const noexcept;
    [[nodiscard]] const std::string& message() const noexcept;

private:
    ErrorCode m_code;
    std::string m_message;
};

/// Result of an operation that can fail recoverably.
template <typename T>
using Expected = std::expected<T, Error>;

/// Builds the error side of an Expected: `return makeUnexpected(ErrorCode::NotFound, "…");`
[[nodiscard]] std::unexpected<Error> makeUnexpected(ErrorCode code, std::string message = {});

} // namespace coil::foundation

namespace coil {
using foundation::Error;
using foundation::ErrorCode;
using foundation::Expected;
using foundation::makeUnexpected;
} // namespace coil

/// Formats as the enumerator name, same as toString().
template <>
struct std::formatter<coil::foundation::ErrorCode> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(coil::foundation::ErrorCode /*code*/, FormatContext& ctx) const {
        return std::formatter<std::string_view>::format(std::string_view{},
                                                        ctx); // SPEC-0002 Red stub
    }
};

/// Formats as "<Code>: <message>", or "<Code>" when the message is empty.
template <>
struct std::formatter<coil::foundation::Error> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const coil::foundation::Error& /*error*/, FormatContext& ctx) const {
        return std::formatter<std::string_view>::format(std::string_view{},
                                                        ctx); // SPEC-0002 Red stub
    }
};
