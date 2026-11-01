#include <coil/platform/testing/fake_window_system.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/platform/window_system_contract.hpp>

#include <gtest/gtest.h>

#include <memory>

namespace coil::platform {

template <>
struct WindowSystemContractTraits<testing::FakeWindowSystem> {
    static std::unique_ptr<WindowSystem> make() {
        return std::make_unique<testing::FakeWindowSystem>();
    }
};

using FakeImplementations = ::testing::Types<testing::FakeWindowSystem>;
INSTANTIATE_TYPED_TEST_SUITE_P(Fake, WindowSystemContract, FakeImplementations);

} // namespace coil::platform
