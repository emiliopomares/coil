#include <coil/foundation/error.hpp>
#include <coil/platform/window_system.hpp>
#include <coil/platform_sdl3/sdl_window_system.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace coil::platform_sdl3 {

namespace {

// SPEC-0005 Red stub: does not touch SDL; stores nothing and reports success with neutral values.
class SdlWindowSystem final : public platform::WindowSystem {
public:
    SdlWindowSystem() = default;
    ~SdlWindowSystem() override = default;

    SdlWindowSystem(const SdlWindowSystem&) = delete;
    SdlWindowSystem& operator=(const SdlWindowSystem&) = delete;
    SdlWindowSystem(SdlWindowSystem&&) = delete;
    SdlWindowSystem& operator=(SdlWindowSystem&&) = delete;

    [[nodiscard]] Expected<platform::WindowId>
    createWindow(const platform::WindowDesc& /*desc*/) override {
        return platform::WindowId{};
    }

    [[nodiscard]] Expected<void> destroyWindow(platform::WindowId /*id*/) override { return {}; }

    [[nodiscard]] std::vector<platform::WindowId> windows() const override { return {}; }

    [[nodiscard]] Expected<std::string> title(platform::WindowId /*id*/) const override {
        return std::string{};
    }

    [[nodiscard]] Expected<void> setTitle(platform::WindowId /*id*/,
                                          std::string_view /*title*/) override {
        return {};
    }

    [[nodiscard]] Expected<platform::WindowSize> size(platform::WindowId /*id*/) const override {
        return platform::WindowSize{};
    }
};

} // namespace

Expected<std::unique_ptr<platform::WindowSystem>>
createSdlWindowSystem(const SdlWindowSystemOptions& /*options*/) {
    return std::make_unique<SdlWindowSystem>();
}

} // namespace coil::platform_sdl3
