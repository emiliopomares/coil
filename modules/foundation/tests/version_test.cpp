#include <coil/foundation/version.hpp>

#include <gtest/gtest.h>

#include <format>
#include <string_view>

namespace {

using coil::foundation::kVersion;
using coil::foundation::Version;
using coil::foundation::versionString;

constexpr Version kExpectedVersion{COIL_TEST_EXPECTED_VERSION_MAJOR,
                                   COIL_TEST_EXPECTED_VERSION_MINOR,
                                   COIL_TEST_EXPECTED_VERSION_PATCH};

constexpr std::string_view kExpectedVersionString{COIL_TEST_EXPECTED_VERSION_STRING};

TEST(Version, AC2_MatchesProjectVersion) {
    EXPECT_EQ(kVersion.major, kExpectedVersion.major);
    EXPECT_EQ(kVersion.minor, kExpectedVersion.minor);
    EXPECT_EQ(kVersion.patch, kExpectedVersion.patch);
    EXPECT_TRUE(kVersion == kExpectedVersion);
}

TEST(Version, AC3_StringMatchesComponents) {
    EXPECT_EQ(versionString(), kExpectedVersionString);
    EXPECT_EQ(versionString(),
              std::format("{}.{}.{}", kVersion.major, kVersion.minor, kVersion.patch));
}

} // namespace
