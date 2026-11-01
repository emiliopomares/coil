#include <coil/foundation/error.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace coil::foundation {

// SPEC-0002 Red stubs: compile, but return neutral or wrong values.

std::string_view toString(ErrorCode /*code*/) noexcept {
    return {};
}

Error::Error(ErrorCode /*code*/, std::string /*message*/)
    : m_code{ErrorCode::Internal} {}

ErrorCode Error::code() const noexcept {
    return m_code;
}

const std::string& Error::message() const noexcept {
    return m_message;
}

std::unexpected<Error> makeUnexpected(ErrorCode /*code*/, std::string /*message*/) {
    return std::unexpected(Error{ErrorCode::Internal});
}

} // namespace coil::foundation
