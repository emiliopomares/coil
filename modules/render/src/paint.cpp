#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/render/color.hpp>
#include <coil/render/paint.hpp>

#include <cmath>
#include <expected>
#include <utility>
#include <vector>

namespace coil::render {

namespace {

bool isFinite(Point point) {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

bool isFinite(Color color) {
    return std::isfinite(color.r) && std::isfinite(color.g) && std::isfinite(color.b) &&
           std::isfinite(color.a);
}

/// At least two stops; offsets finite, in [0, 1] and non-decreasing; colours finite.
bool areValid(const std::vector<GradientStop>& stops) {
    if (stops.size() < 2) {
        return false;
    }
    float previous = 0.0F;
    for (const GradientStop& stop : stops) {
        if (std::isnan(stop.offset) || stop.offset < previous || stop.offset > 1.0F ||
            !isFinite(stop.color)) {
            return false;
        }
        previous = stop.offset;
    }
    return true;
}

std::unexpected<Error> invalidGradient() {
    return makeUnexpected(ErrorCode::InvalidArgument,
                          "a gradient needs >= 2 ordered stops in [0, 1] and finite geometry");
}

} // namespace

Expected<Paint> linearGradient(Point start, Point end, std::vector<GradientStop> stops) {
    if (!areValid(stops) || !isFinite(start) || !isFinite(end)) {
        return invalidGradient();
    }
    return Paint{LinearGradient{.start = start, .end = end, .stops = std::move(stops)}};
}

Expected<Paint> radialGradient(Point center, float radius, std::vector<GradientStop> stops) {
    if (!areValid(stops) || !isFinite(center) || !std::isfinite(radius) || radius <= 0.0F) {
        return invalidGradient();
    }
    return Paint{RadialGradient{.center = center, .radius = radius, .stops = std::move(stops)}};
}

Expected<Paint> sweepGradient(Point center, float startAngle, std::vector<GradientStop> stops) {
    if (!areValid(stops) || !isFinite(center) || !std::isfinite(startAngle)) {
        return invalidGradient();
    }
    return Paint{
        SweepGradient{.center = center, .startAngle = startAngle, .stops = std::move(stops)}};
}

} // namespace coil::render
