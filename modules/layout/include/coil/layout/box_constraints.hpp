#pragma once

#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>

#include <format>
#include <limits>
#include <string>
#include <string_view>

namespace coil::layout {

/// No upper bound on an axis.
inline constexpr float kUnbounded = std::numeric_limits<float>::infinity();

/// The sizes a parent allows a child to take (Flutter model, ADR-0019): for each axis a minimum
/// (finite, >= 0) and a maximum (>= the minimum, possibly kUnbounded). Every BoxConstraints value
/// satisfies these invariants; the factories reject anything else.
class BoxConstraints {
public:
    /// Any size from 0x0 to unbounded on both axes. Same as unbounded().
    constexpr BoxConstraints() noexcept = default;

    /// Fails with InvalidArgument if any value is NaN, a minimum is negative or infinite, or a
    /// minimum exceeds its maximum.
    [[nodiscard]] static Expected<BoxConstraints> create(float minWidth, float maxWidth,
                                                         float minHeight, float maxHeight);
    /// Exactly `size`. Fails with InvalidArgument unless both sides are finite and >= 0.
    [[nodiscard]] static Expected<BoxConstraints> tight(Size size);
    /// From 0x0 up to `size`. Fails with InvalidArgument if a side is NaN or negative; kUnbounded
    /// sides are allowed.
    [[nodiscard]] static Expected<BoxConstraints> loose(Size size);

    /// From 0x0 to unbounded on both axes.
    [[nodiscard]] static constexpr BoxConstraints unbounded() noexcept { return {}; }

    [[nodiscard]] float minWidth() const noexcept;
    [[nodiscard]] float maxWidth() const noexcept;
    [[nodiscard]] float minHeight() const noexcept;
    [[nodiscard]] float maxHeight() const noexcept;

    /// True when min == max on both axes.
    [[nodiscard]] bool isTight() const noexcept;
    /// True when maxWidth is finite.
    [[nodiscard]] bool hasBoundedWidth() const noexcept;
    /// True when maxHeight is finite.
    [[nodiscard]] bool hasBoundedHeight() const noexcept;

    /// The size closest to `size` that these constraints allow: each side clamped into [min, max],
    /// a NaN side becomes the minimum.
    [[nodiscard]] Size constrain(Size size) const noexcept;
    /// True if `size` lies within the constraints on both axes.
    [[nodiscard]] bool isSatisfiedBy(Size size) const noexcept;
    /// The constraints left for content inside `insets`: the insets are subtracted from min and
    /// max on each axis, floored at 0. kUnbounded stays kUnbounded; a NaN inset counts as 0, and
    /// negative insets enlarge.
    [[nodiscard]] BoxConstraints deflate(Insets insets) const noexcept;
    /// Same maximums, minimums set to 0.
    [[nodiscard]] BoxConstraints loosen() const noexcept;
    /// `other`, clamped into these constraints: each of other's minimum and maximum is clamped into
    /// [min, max] of this one, per axis. The result always satisfies these constraints.
    [[nodiscard]] BoxConstraints enforce(const BoxConstraints& other) const noexcept;

    friend bool operator==(const BoxConstraints&, const BoxConstraints&) = default;

private:
    constexpr BoxConstraints(float minWidth, float maxWidth, float minHeight,
                             float maxHeight) noexcept
        : m_minWidth(minWidth)
        , m_maxWidth(maxWidth)
        , m_minHeight(minHeight)
        , m_maxHeight(maxHeight) {}

    float m_minWidth = 0.0F;
    float m_maxWidth = kUnbounded;
    float m_minHeight = 0.0F;
    float m_maxHeight = kUnbounded;
};

} // namespace coil::layout

/// Formats as "w 0..100, h 20..inf".
template <>
struct std::formatter<coil::layout::BoxConstraints> : std::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const coil::layout::BoxConstraints& constraints, FormatContext& ctx) const {
        const std::string text =
            std::format("w {}..{}, h {}..{}", constraints.minWidth(), constraints.maxWidth(),
                        constraints.minHeight(), constraints.maxHeight());
        return std::formatter<std::string_view>::format(text, ctx);
    }
};
