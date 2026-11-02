#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/render/paint.hpp>

#include <utility>
#include <vector>

namespace coil::render {

Expected<Paint> linearGradient(Point /*start*/, Point /*end*/, std::vector<GradientStop> stops) {
    return Paint{LinearGradient{.start = {}, .end = {}, .stops = std::move(stops)}};
}

Expected<Paint> radialGradient(Point /*center*/, float /*radius*/,
                               std::vector<GradientStop> stops) {
    return Paint{RadialGradient{.center = {}, .radius = 0.0F, .stops = std::move(stops)}};
}

Expected<Paint> sweepGradient(Point /*center*/, float /*startAngle*/,
                              std::vector<GradientStop> stops) {
    return Paint{SweepGradient{.center = {}, .startAngle = 0.0F, .stops = std::move(stops)}};
}

} // namespace coil::render
