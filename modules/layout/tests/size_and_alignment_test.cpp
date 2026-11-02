#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/alignment.hpp>
#include <coil/layout/box_constraints.hpp>
#include <coil/layout/layout_element.hpp>
#include <coil/layout/modifier.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <optional>
#include <ostream>
#include <string_view>
#include <utility>

namespace coil::foundation {
inline namespace {

// Readable failure messages (test-only; GoogleTest uses operator<<).
std::ostream& operator<<(std::ostream& os, Size size) {
    return os << "Size{" << size.width << ", " << size.height << "}";
}

std::ostream& operator<<(std::ostream& os, Rect rect) {
    return os << "Rect{" << rect.origin.x << ", " << rect.origin.y << ", " << rect.size.width
              << ", " << rect.size.height << "}";
}

} // namespace
} // namespace coil::foundation

namespace coil::layout {
inline namespace {

std::ostream& operator<<(std::ostream& os, const BoxConstraints& constraints) {
    return os << "BoxConstraints{w " << constraints.minWidth() << ".." << constraints.maxWidth()
              << ", h " << constraints.minHeight() << ".." << constraints.maxHeight() << "}";
}

} // namespace
} // namespace coil::layout

namespace {

using coil::foundation::ErrorCode;
using coil::foundation::Expected;
using coil::foundation::Rect;
using coil::foundation::Size;
using coil::layout::align;
using coil::layout::Alignment;
using coil::layout::BoxConstraints;
using coil::layout::center;
using coil::layout::kUnbounded;
using coil::layout::LayoutElement;
using coil::layout::maxSize;
using coil::layout::minSize;
using coil::layout::ModifierChain;
using coil::layout::ModifierResult;

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

Size dims(float width, float height) {
    return Size{.width = width, .height = height};
}

Rect rect(float x, float y, float width, float height) {
    return Rect{.origin = {.x = x, .y = y}, .size = {.width = width, .height = height}};
}

BoxConstraints constraints(float minWidth, float maxWidth, float minHeight, float maxHeight) {
    const Expected<BoxConstraints> result =
        BoxConstraints::create(minWidth, maxWidth, minHeight, maxHeight);
    EXPECT_TRUE(result.has_value());
    return result.value_or(BoxConstraints{});
}

BoxConstraints loose(float width, float height) {
    return constraints(0.0F, width, 0.0F, height);
}

BoxConstraints tight(float width, float height) {
    return constraints(width, width, height, height);
}

/// A leaf that prefers a given size and records what it was given (as in SPEC-0016's tests).
class Leaf final : public LayoutElement {
public:
    explicit Leaf(Size preferred)
        : m_preferred(preferred) {}

    [[nodiscard]] const std::optional<BoxConstraints>& seenConstraints() const noexcept {
        return m_seenConstraints;
    }

protected:
    [[nodiscard]] Size measureContent(const BoxConstraints& constraints) override {
        m_seenConstraints = constraints;
        return m_preferred;
    }

    void arrangeContent(Rect /*contentBounds*/) override {}

private:
    Size m_preferred;
    std::optional<BoxConstraints> m_seenConstraints;
};

/// A 50x30 leaf with a single modifier.
class Modified {
public:
    explicit Modified(ModifierResult modifier) {
        const Expected<void> result =
            m_leaf.setModifiers(ModifierChain{}.then(std::move(modifier)));
        EXPECT_TRUE(result.has_value()) << "setModifiers failed";
    }

    [[nodiscard]] Leaf& leaf() noexcept { return m_leaf; }

private:
    Leaf m_leaf{dims(50.0F, 30.0F)};
};

void expectInvalid(const ModifierResult& result, std::string_view what) {
    ASSERT_FALSE(result.has_value()) << what << " should fail";
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument) << what;
}

std::string_view typeIdOf(const ModifierResult& result) {
    if (!result.has_value() || *result == nullptr) {
        return "<none>";
    }
    return (*result)->typeId();
}

TEST(SizeModifiers, AC2_SizeAsksForExactSize) {
    Modified element{coil::layout::size(dims(80.0F, 40.0F))};
    Leaf& leaf = element.leaf();

    EXPECT_EQ(leaf.measure(loose(100.0F, 100.0F)), dims(80.0F, 40.0F));
    EXPECT_EQ(leaf.seenConstraints(), tight(80.0F, 40.0F));

    EXPECT_EQ(leaf.measure(tight(60.0F, 60.0F)), dims(60.0F, 60.0F));
    EXPECT_EQ(leaf.seenConstraints(), tight(60.0F, 60.0F));
}

TEST(SizeModifiers, AC3_MinSizeRaisesMinimums) {
    Modified element{minSize(dims(70.0F, 20.0F))};
    Leaf& leaf = element.leaf();

    EXPECT_EQ(leaf.measure(loose(100.0F, 100.0F)), dims(70.0F, 30.0F));
    EXPECT_EQ(leaf.seenConstraints(), constraints(70.0F, 100.0F, 20.0F, 100.0F));

    EXPECT_EQ(leaf.measure(loose(60.0F, 60.0F)), dims(60.0F, 30.0F));
    EXPECT_EQ(leaf.seenConstraints(), constraints(60.0F, 60.0F, 20.0F, 60.0F));
}

TEST(SizeModifiers, AC4_MaxSizeLowersMaximums) {
    Modified element{maxSize(dims(40.0F, kUnbounded))};
    Leaf& leaf = element.leaf();

    EXPECT_EQ(leaf.measure(loose(100.0F, 100.0F)), dims(40.0F, 30.0F));
    EXPECT_EQ(leaf.seenConstraints(), loose(40.0F, 100.0F));

    EXPECT_EQ(leaf.measure(tight(80.0F, 80.0F)), dims(80.0F, 80.0F));
    EXPECT_EQ(leaf.seenConstraints(), tight(80.0F, 80.0F));
}

TEST(Align, AC5_PositionsContentInTightSpace) {
    const auto contentAt = [](ModifierResult modifier) {
        Modified element{std::move(modifier)};
        Leaf& leaf = element.leaf();
        EXPECT_EQ(leaf.measure(tight(200.0F, 100.0F)), dims(200.0F, 100.0F));
        EXPECT_EQ(leaf.seenConstraints(), loose(200.0F, 100.0F));
        leaf.arrange(rect(0.0F, 0.0F, 200.0F, 100.0F));
        return leaf.contentBounds();
    };

    EXPECT_EQ(contentAt(align(Alignment::bottomEnd())), rect(150.0F, 70.0F, 50.0F, 30.0F));
    EXPECT_EQ(contentAt(center()), rect(75.0F, 35.0F, 50.0F, 30.0F));
    EXPECT_EQ(contentAt(align(Alignment::topStart())), rect(0.0F, 0.0F, 50.0F, 30.0F));
    EXPECT_EQ(contentAt(align(Alignment{.horizontal = -0.5F, .vertical = 0.5F})),
              rect(37.5F, 52.5F, 50.0F, 30.0F));
}

TEST(Align, AC6_WrapsContentInLooseSpace) {
    Modified element{center()};
    Leaf& leaf = element.leaf();

    EXPECT_EQ(leaf.measure(loose(200.0F, 100.0F)), dims(50.0F, 30.0F));
    leaf.arrange(rect(10.0F, 10.0F, 50.0F, 30.0F));
    EXPECT_EQ(leaf.contentBounds(), rect(10.0F, 10.0F, 50.0F, 30.0F));
    leaf.arrange(rect(0.0F, 0.0F, 20.0F, 20.0F));
    EXPECT_EQ(leaf.contentBounds(), rect(-15.0F, -5.0F, 50.0F, 30.0F));
}

TEST(SizeModifiers, AC7_InvalidParametersAreRejected) {
    expectInvalid(coil::layout::size(dims(-1.0F, 10.0F)), "size, negative");
    expectInvalid(coil::layout::size(dims(10.0F, kNaN)), "size, NaN");
    expectInvalid(coil::layout::size(dims(kUnbounded, 10.0F)), "size, infinite");
    expectInvalid(minSize(dims(-1.0F, 10.0F)), "minSize, negative");
    expectInvalid(minSize(dims(10.0F, kNaN)), "minSize, NaN");
    expectInvalid(minSize(dims(10.0F, kUnbounded)), "minSize, infinite");
    expectInvalid(maxSize(dims(-1.0F, 10.0F)), "maxSize, negative");
    expectInvalid(maxSize(dims(10.0F, kNaN)), "maxSize, NaN");
    EXPECT_TRUE(maxSize(dims(kUnbounded, kUnbounded)).has_value());
    expectInvalid(align(Alignment{.horizontal = kNaN, .vertical = 0.0F}), "align, NaN");
    expectInvalid(align(Alignment{.horizontal = 0.0F, .vertical = kUnbounded}), "align, infinite");
    expectInvalid(align(Alignment{.horizontal = -1.5F, .vertical = 0.0F}), "align, -1.5");
    expectInvalid(align(Alignment{.horizontal = 0.0F, .vertical = 1.5F}), "align, 1.5");
}

TEST(SizeModifiers, AC8_TypeIds) {
    EXPECT_EQ(typeIdOf(coil::layout::size(dims(1.0F, 1.0F))), "coil.size");
    EXPECT_EQ(typeIdOf(minSize(dims(1.0F, 1.0F))), "coil.minSize");
    EXPECT_EQ(typeIdOf(maxSize(dims(1.0F, 1.0F))), "coil.maxSize");
    EXPECT_EQ(typeIdOf(align(Alignment::topEnd())), "coil.align");
    EXPECT_EQ(typeIdOf(center()), "coil.align");
}

} // namespace
