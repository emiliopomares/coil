#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/box_constraints.hpp>
#include <coil/layout/layout_element.hpp>
#include <coil/layout/modifier.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <string_view>
#include <utility>
#include <vector>

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
using coil::foundation::Insets;
using coil::foundation::Rect;
using coil::foundation::Size;
using coil::layout::BoxConstraints;
using coil::layout::kUnbounded;
using coil::layout::LayoutElement;
using coil::layout::LayoutModifier;
using coil::layout::layoutRoot;
using coil::layout::ModifierChain;
using coil::layout::ModifierResult;
using coil::layout::padding;

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

Size size(float width, float height) {
    return Size{.width = width, .height = height};
}

Rect rect(float x, float y, float width, float height) {
    return Rect{.origin = {.x = x, .y = y}, .size = {.width = width, .height = height}};
}

Insets insets(float left, float top, float right, float bottom) {
    return Insets{.left = left, .top = top, .right = right, .bottom = bottom};
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

// --- Test-defined elements and modifiers: ordinary clients of the public protocol. ---

/// A leaf that prefers a given size and records what it was given.
class Leaf final : public LayoutElement {
public:
    explicit Leaf(Size preferred)
        : m_preferred(preferred) {}

    [[nodiscard]] const std::optional<BoxConstraints>& seenConstraints() const noexcept {
        return m_seenConstraints;
    }

    [[nodiscard]] const std::optional<Rect>& arrangedContent() const noexcept {
        return m_arrangedContent;
    }

protected:
    [[nodiscard]] Size measureContent(const BoxConstraints& constraints) override {
        m_seenConstraints = constraints;
        return m_preferred;
    }

    void arrangeContent(Rect contentBounds) override { m_arrangedContent = contentBounds; }

private:
    Size m_preferred;
    std::optional<BoxConstraints> m_seenConstraints;
    std::optional<Rect> m_arrangedContent;
};

/// A custom container: measures each child under loose constraints of its own maximums, stacks them
/// top to bottom at their measured sizes, and fills its constraints itself.
class Stack final : public LayoutElement {
public:
    void add(std::unique_ptr<LayoutElement> child) { m_children.push_back(std::move(child)); }

protected:
    [[nodiscard]] Size measureContent(const BoxConstraints& constraints) override {
        const BoxConstraints childConstraints = constraints.loosen();
        for (const auto& child : m_children) {
            static_cast<void>(child->measure(childConstraints));
        }
        return size(constraints.maxWidth(), constraints.maxHeight());
    }

    void arrangeContent(Rect contentBounds) override {
        float top = contentBounds.origin.y;
        for (const auto& child : m_children) {
            const Size childSize = child->measuredSize();
            child->arrange(rect(contentBounds.origin.x, top, childSize.width, childSize.height));
            top += childSize.height;
        }
    }

private:
    std::vector<std::unique_ptr<LayoutElement>> m_children;
};

/// A custom modifier that forces its inside to an exact size.
class FixedSize final : public LayoutModifier {
public:
    explicit FixedSize(Size fixed)
        : m_fixed(fixed) {}

    [[nodiscard]] std::string_view typeId() const noexcept override { return "test.fixedSize"; }

    [[nodiscard]] BoxConstraints
    innerConstraints(const BoxConstraints& outer) const noexcept override {
        return BoxConstraints::tight(m_fixed).value_or(outer);
    }

    [[nodiscard]] Size outerSize(const BoxConstraints& /*outer*/,
                                 Size /*inner*/) const noexcept override {
        return m_fixed;
    }

    [[nodiscard]] Rect innerBounds(Rect outer, Size /*inner*/) const noexcept override {
        return outer;
    }

private:
    Size m_fixed;
};

ModifierResult fixedSize(float width, float height) {
    return std::make_unique<FixedSize>(size(width, height));
}

void setChain(LayoutElement& element, ModifierChain chain) {
    const Expected<void> result = element.setModifiers(std::move(chain));
    ASSERT_TRUE(result.has_value()) << "setModifiers failed";
}

void expectInvalid(const ModifierResult& result, std::string_view what) {
    ASSERT_FALSE(result.has_value()) << what << " should fail";
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument) << what;
}

TEST(LayoutElement, AC1_MeasuresAndArrangesContent) {
    Leaf leaf{size(50.0F, 30.0F)};
    EXPECT_EQ(leaf.measuredSize(), Size{});
    EXPECT_EQ(leaf.bounds(), Rect{});
    EXPECT_EQ(leaf.contentBounds(), Rect{});

    EXPECT_EQ(leaf.measure(loose(100.0F, 100.0F)), size(50.0F, 30.0F));
    EXPECT_EQ(leaf.seenConstraints(), loose(100.0F, 100.0F));
    EXPECT_EQ(leaf.measuredSize(), size(50.0F, 30.0F));

    EXPECT_EQ(leaf.measure(tight(80.0F, 80.0F)), size(80.0F, 80.0F));
    EXPECT_EQ(leaf.measuredSize(), size(80.0F, 80.0F));

    leaf.arrange(rect(10.0F, 20.0F, 80.0F, 80.0F));
    EXPECT_EQ(leaf.bounds(), rect(10.0F, 20.0F, 80.0F, 80.0F));
    EXPECT_EQ(leaf.contentBounds(), rect(10.0F, 20.0F, 80.0F, 80.0F));
    EXPECT_EQ(leaf.arrangedContent(), rect(10.0F, 20.0F, 80.0F, 80.0F));
}

TEST(LayoutElement, AC2_ContentSizesAreClamped) {
    Leaf odd{size(kNaN, 500.0F)};
    EXPECT_EQ(odd.measure(constraints(10.0F, 100.0F, 20.0F, 200.0F)), size(10.0F, 200.0F));

    Leaf infinite{size(kUnbounded, kUnbounded)};
    EXPECT_EQ(infinite.measure(BoxConstraints::unbounded()), size(0.0F, 0.0F));
    EXPECT_EQ(infinite.measuredSize(), size(0.0F, 0.0F));
}

TEST(Padding, AC3_ShrinksConstraintsAndGrowsSize) {
    Leaf leaf{size(50.0F, 30.0F)};
    setChain(leaf, ModifierChain{}.then(padding(insets(5.0F, 10.0F, 15.0F, 20.0F))));

    EXPECT_EQ(leaf.measure(loose(100.0F, 100.0F)), size(70.0F, 60.0F));
    EXPECT_EQ(leaf.seenConstraints(), loose(80.0F, 70.0F));

    EXPECT_EQ(leaf.measure(tight(100.0F, 100.0F)), size(100.0F, 100.0F));
    EXPECT_EQ(leaf.seenConstraints(), tight(80.0F, 70.0F));
}

TEST(Padding, AC4_InsetsTheContent) {
    Leaf leaf{size(50.0F, 30.0F)};
    setChain(leaf, ModifierChain{}.then(padding(insets(5.0F, 10.0F, 15.0F, 20.0F))));
    EXPECT_EQ(leaf.measure(loose(100.0F, 100.0F)), size(70.0F, 60.0F));

    leaf.arrange(rect(10.0F, 10.0F, 70.0F, 60.0F));
    EXPECT_EQ(leaf.bounds(), rect(10.0F, 10.0F, 70.0F, 60.0F));
    EXPECT_EQ(leaf.contentBounds(), rect(15.0F, 20.0F, 50.0F, 30.0F));
    EXPECT_EQ(leaf.arrangedContent(), rect(15.0F, 20.0F, 50.0F, 30.0F));

    leaf.arrange(rect(0.0F, 0.0F, 10.0F, 10.0F));
    EXPECT_EQ(leaf.contentBounds(), rect(5.0F, 10.0F, 0.0F, 0.0F));
}

TEST(ModifierChain, AC5_OrderMattersAndCustomModifiersWork) {
    Leaf paddedOutside{size(50.0F, 30.0F)};
    setChain(paddedOutside, ModifierChain{}.then(padding(8.0F)).then(fixedSize(100.0F, 100.0F)));
    EXPECT_EQ(paddedOutside.modifiers().typeIds(),
              (std::vector<std::string_view>{"coil.padding", "test.fixedSize"}));
    EXPECT_EQ(paddedOutside.measure(BoxConstraints::unbounded()), size(116.0F, 116.0F));
    paddedOutside.arrange(rect(0.0F, 0.0F, 116.0F, 116.0F));
    EXPECT_EQ(paddedOutside.contentBounds(), rect(8.0F, 8.0F, 100.0F, 100.0F));

    Leaf paddedInside{size(50.0F, 30.0F)};
    setChain(paddedInside, ModifierChain{}.then(fixedSize(100.0F, 100.0F)).then(padding(8.0F)));
    EXPECT_EQ(paddedInside.modifiers().typeIds(),
              (std::vector<std::string_view>{"test.fixedSize", "coil.padding"}));
    EXPECT_EQ(paddedInside.measure(BoxConstraints::unbounded()), size(100.0F, 100.0F));
    EXPECT_EQ(paddedInside.seenConstraints(), tight(84.0F, 84.0F));
    paddedInside.arrange(rect(0.0F, 0.0F, 100.0F, 100.0F));
    EXPECT_EQ(paddedInside.contentBounds(), rect(8.0F, 8.0F, 84.0F, 84.0F));
}

TEST(ModifierChain, AC6_InvalidModifiersAreRejected) {
    expectInvalid(padding(insets(-1.0F, 0.0F, 0.0F, 0.0F)), "negative left");
    expectInvalid(padding(insets(0.0F, kNaN, 0.0F, 0.0F)), "NaN top");
    expectInvalid(padding(insets(0.0F, 0.0F, kUnbounded, 0.0F)), "infinite right");
    expectInvalid(padding(insets(0.0F, 0.0F, 0.0F, -kUnbounded)), "-infinite bottom");
    expectInvalid(padding(-1.0F), "negative all");
    expectInvalid(padding(kNaN), "NaN all");
    expectInvalid(padding(kUnbounded), "infinite all");

    ModifierChain bad =
        ModifierChain{}.then(padding(4.0F)).then(padding(-1.0F)).then(fixedSize(10.0F, 10.0F));
    const std::optional<coil::foundation::Error>& error = bad.error();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error.value_or(coil::foundation::Error{ErrorCode::Internal}).code(),
              ErrorCode::InvalidArgument);
    EXPECT_EQ(bad.typeIds(), (std::vector<std::string_view>{"coil.padding"}));

    Leaf leaf{size(50.0F, 30.0F)};
    setChain(leaf, ModifierChain{}.then(padding(10.0F)));
    EXPECT_EQ(leaf.measure(loose(100.0F, 100.0F)), size(70.0F, 50.0F));

    const Expected<void> result = leaf.setModifiers(std::move(bad));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument);
    EXPECT_EQ(leaf.modifiers().typeIds(), (std::vector<std::string_view>{"coil.padding"}));
    EXPECT_EQ(leaf.measure(loose(100.0F, 100.0F)), size(70.0F, 50.0F));
}

TEST(LayoutElement, AC7_CustomContainerLaysOutChildren) {
    Stack stack;
    auto padded = std::make_unique<Leaf>(size(50.0F, 30.0F));
    Leaf& first = *padded;
    setChain(first, ModifierChain{}.then(padding(10.0F)));
    auto plain = std::make_unique<Leaf>(size(40.0F, 20.0F));
    const Leaf& second = *plain;
    stack.add(std::move(padded));
    stack.add(std::move(plain));

    ASSERT_TRUE(layoutRoot(stack, size(200.0F, 100.0F)).has_value());
    EXPECT_EQ(stack.bounds(), rect(0.0F, 0.0F, 200.0F, 100.0F));
    EXPECT_EQ(first.bounds(), rect(0.0F, 0.0F, 70.0F, 50.0F));
    EXPECT_EQ(first.contentBounds(), rect(10.0F, 10.0F, 50.0F, 30.0F));
    EXPECT_EQ(second.bounds(), rect(0.0F, 50.0F, 40.0F, 20.0F));
}

TEST(LayoutElement, AC8_LayoutRootRejectsInvalidSizes) {
    Leaf leaf{size(50.0F, 30.0F)};
    ASSERT_TRUE(layoutRoot(leaf, size(100.0F, 50.0F)).has_value());
    EXPECT_EQ(leaf.measuredSize(), size(100.0F, 50.0F));
    EXPECT_EQ(leaf.bounds(), rect(0.0F, 0.0F, 100.0F, 50.0F));

    const std::vector<Size> invalid{size(-1.0F, 50.0F), size(100.0F, kNaN),
                                    size(kUnbounded, 50.0F)};
    for (const Size bad : invalid) {
        const Expected<void> result = layoutRoot(leaf, bad);
        ASSERT_FALSE(result.has_value()) << bad;
        EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument) << bad;
        EXPECT_EQ(leaf.measuredSize(), size(100.0F, 50.0F)) << bad;
        EXPECT_EQ(leaf.bounds(), rect(0.0F, 0.0F, 100.0F, 50.0F)) << bad;
    }
}

} // namespace
