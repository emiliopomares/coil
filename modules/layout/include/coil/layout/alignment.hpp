#pragma once

namespace coil::layout {

/// A position inside a box: -1 is the start (left) or top edge, 0 the centre, 1 the end (right) or
/// bottom edge. Values in between are proportional. Start and end mean left and right until
/// right-to-left layout is specified.
struct Alignment {
    float horizontal = 0.0F;
    float vertical = 0.0F;

    [[nodiscard]] static constexpr Alignment topStart() noexcept {
        return {.horizontal = -1.0F, .vertical = -1.0F};
    }

    [[nodiscard]] static constexpr Alignment topCenter() noexcept {
        return {.horizontal = 0.0F, .vertical = -1.0F};
    }

    [[nodiscard]] static constexpr Alignment topEnd() noexcept {
        return {.horizontal = 1.0F, .vertical = -1.0F};
    }

    [[nodiscard]] static constexpr Alignment centerStart() noexcept {
        return {.horizontal = -1.0F, .vertical = 0.0F};
    }

    [[nodiscard]] static constexpr Alignment center() noexcept {
        return {.horizontal = 0.0F, .vertical = 0.0F};
    }

    [[nodiscard]] static constexpr Alignment centerEnd() noexcept {
        return {.horizontal = 1.0F, .vertical = 0.0F};
    }

    [[nodiscard]] static constexpr Alignment bottomStart() noexcept {
        return {.horizontal = -1.0F, .vertical = 1.0F};
    }

    [[nodiscard]] static constexpr Alignment bottomCenter() noexcept {
        return {.horizontal = 0.0F, .vertical = 1.0F};
    }

    [[nodiscard]] static constexpr Alignment bottomEnd() noexcept {
        return {.horizontal = 1.0F, .vertical = 1.0F};
    }

    friend constexpr bool operator==(Alignment, Alignment) = default;
};

} // namespace coil::layout
