#include <coil/foundation/version.hpp>
#include <coil/foundation/version_config.hpp>

#include <string_view>

namespace coil::foundation {

std::string_view versionString() noexcept {
    return detail::kVersionString;
}

} // namespace coil::foundation
