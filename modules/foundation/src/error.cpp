#include <coil/foundation/error.hpp>

#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace coil::foundation {

std::string_view toString(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::InvalidArgument:
        return "InvalidArgument";
    case ErrorCode::NotFound:
        return "NotFound";
    case ErrorCode::Unavailable:
        return "Unavailable";
    case ErrorCode::Internal:
        return "Internal";
    case ErrorCode::Cancelled:
        break; // Red stub (SPEC-0024)
    }
    return "Unknown";
}

Error::Error(ErrorCode code, std::string message)
    : m_code{code}
    , m_message{std::move(message)} {}

ErrorCode Error::code() const noexcept {
    return m_code;
}

const std::string& Error::message() const noexcept {
    return m_message;
}

std::unexpected<Error> makeUnexpected(ErrorCode code, std::string message) {
    return std::unexpected(Error{code, std::move(message)});
}

} // namespace coil::foundation
