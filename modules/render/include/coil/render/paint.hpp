#pragma once

#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/render/color.hpp>

#include <variant>
#include <vector>

namespace coil::render {

/// A colour at a position along a gradient.
struct GradientStop {
    float offset = 0.0F; ///< In [0, 1].
    Color color;

    friend bool operator==(const GradientStop&, const GradientStop&) = default;
};

/// Colours along the line from `start` to `end` (DIPs, same space as the shapes).
struct LinearGradient {
    Point start;
    Point end;
    std::vector<GradientStop> stops;

    friend bool operator==(const LinearGradient&, const LinearGradient&) = default;
};

/// Colours by distance from `center`, reaching the last stop at `radius`.
struct RadialGradient {
    Point center;
    float radius = 0.0F;
    std::vector<GradientStop> stops;

    friend bool operator==(const RadialGradient&, const RadialGradient&) = default;
};

/// Colours by angle around `center`: one full turn, starting at `startAngle` (radians; 0 points to
/// +x, angles grow clockwise because y grows downwards).
struct SweepGradient {
    Point center;
    float startAngle = 0.0F;
    std::vector<GradientStop> stops;

    friend bool operator==(const SweepGradient&, const SweepGradient&) = default;
};

/// What a shape is filled with (ADR-0009 Amendment 2).
using Paint = std::variant<Color, LinearGradient, RadialGradient, SweepGradient>;

/// A linear gradient. Fails with InvalidArgument unless the stops are valid (at least two; offsets
/// finite, in [0, 1] and non-decreasing; colours finite) and both points are finite.
[[nodiscard]] Expected<Paint> linearGradient(Point start, Point end,
                                             std::vector<GradientStop> stops);
/// A radial gradient. Fails with InvalidArgument unless the stops are valid, the centre is finite
/// and the radius is finite and > 0.
[[nodiscard]] Expected<Paint> radialGradient(Point center, float radius,
                                             std::vector<GradientStop> stops);
/// A sweep gradient. Fails with InvalidArgument unless the stops are valid and the centre and
/// start angle are finite.
[[nodiscard]] Expected<Paint> sweepGradient(Point center, float startAngle,
                                            std::vector<GradientStop> stops);

} // namespace coil::render
