#include <coil/foundation/error.hpp>
#include <coil/platform/testing/fake_window_system.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/platform/window_system_contract.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace coil::platform {

template <>
struct WindowSystemContractTraits<testing::FakeWindowSystem> {
    static std::unique_ptr<WindowSystem> make() {
        return std::make_unique<testing::FakeWindowSystem>();
    }

    static void requestClose(WindowSystem& system, WindowId id) {
        auto& fake = dynamic_cast<testing::FakeWindowSystem&>(system);
        ASSERT_TRUE(succeeds(fake.requestClose(id)));
    }
};

using FakeImplementations = ::testing::Types<testing::FakeWindowSystem>;
INSTANTIATE_TYPED_TEST_SUITE_P(Fake, WindowSystemContract, FakeImplementations);

namespace {

TEST(FakeWindowSystem, AC6_RequestCloseOnUnknownIdIsNotFound) {
    testing::FakeWindowSystem fake;
    EXPECT_TRUE(failsWith(fake.requestClose(WindowId{.value = 999}), ErrorCode::NotFound));
    EXPECT_EQ(fake.pollEvents(), std::vector<WindowEvent>{});
}

} // namespace

} // namespace coil::platform
