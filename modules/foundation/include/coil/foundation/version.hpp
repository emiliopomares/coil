#pragma once

#include <coil/foundation/version_config.hpp>

#include <string_view>

namespace coil::foundation {

/// Semantic version of the Coil library.
struct Version {
    int major;
    int minor;
    int patch;

    friend constexpr bool operator==(const Version&, const Version&) = default;
};

/// Version of the Coil library, generated from the CMake project version.
inline constexpr Version kVersion{
    .major = detail::kVersionMajor, .minor = detail::kVersionMinor, .patch = detail::kVersionPatch};

/// Returns the library version formatted as "MAJOR.MINOR.PATCH".
[[nodiscard]] std::string_view versionString() noexcept;

} // namespace coil::foundation
