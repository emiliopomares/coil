#pragma once

#include <source_location>

namespace coil::foundation::detail {

/// Reports a violated precondition (`condition` is its source text) and aborts. Never returns.
[[noreturn]] void expectsFailed(const char* condition, std::source_location where) noexcept;

} // namespace coil::foundation::detail

/// Checks a precondition in every build. A violation is a programmer error: it is reported on
/// stderr with the condition, file and line, and the program aborts (CLAUDE.md §5).
#define COIL_EXPECTS(condition)                                                                    \
    ((condition)                                                                                   \
         ? static_cast<void>(0)                                                                    \
         : ::coil::foundation::detail::expectsFailed(#condition, std::source_location::current()))
