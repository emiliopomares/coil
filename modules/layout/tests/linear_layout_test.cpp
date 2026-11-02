#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/box_constraints.hpp>
#include <coil/layout/layout_element.hpp>
#include <coil/layout/linear_layout.hpp>
#include <coil/layout/modifier.hpp>

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <memory>
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

namespace {

using coil::foundation::ErrorCode;
using coil::foundation::Expected;
using coil::foundation::Rect;
using coil::foundation::Size;
using coil::layout::BoxConstraints;
using coil::layout::Column;
using coil::layout::CrossAxisAlignment;
using coil::layout::kUnbounded;
using coil::layout::LayoutElement;
using coil::layout::LinearLayoutOptions;
using coil::layout::MainAxisAlignment;
using coil::layout::ModifierChain;
using coil::layout::ModifierResult;
using coil::layout::Row;
using coil::layout::Weight;
using coil::layout::weight;

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

/// A leaf that prefers a given size (as in SPEC-0016's tests).
class Leaf final : public LayoutElement {
public:
    explicit Leaf(Size preferred)
        : m_preferred(preferred) {}

protected:
    [[nodiscard]] Size measureContent(const BoxConstraints& /*constraints*/) override {
        return m_preferred;
    }

    void arrangeContent(Rect /*contentBounds*/) override {}

private:
    Size m_preferred;
};

/// Adds a leaf to `container` and returns it (the container owns it).
template <typename Container>
Leaf& addLeaf(Container& container, Size preferred, ModifierChain chain = {}) {
    auto leaf = std::make_unique<Leaf>(preferred);
    Leaf& result = *leaf;
    EXPECT_TRUE(result.setModifiers(std::move(chain)).has_value()) << "setModifiers failed";
    EXPECT_TRUE(container.add(std::move(leaf)).has_value()) << "add failed";
    return result;
}

/// The three standard children: a 50x30, b 40x20, c 30x40.
template <typename Container>
struct Line {
    Container container;
    Leaf* a = &addLeaf(container, dims(50.0F, 30.0F));
    Leaf* b = &addLeaf(container, dims(40.0F, 20.0F));
    Leaf* c = &addLeaf(container, dims(30.0F, 40.0F));

    explicit Line(LinearLayoutOptions options) {
        EXPECT_TRUE(container.setOptions(options).has_value()) << "setOptions failed";
    }

    /// Measures under `incoming`, then arranges at (0, 0) with the measured size.
    Size layOut(const BoxConstraints& incoming) {
        const Size measured = container.measure(incoming);
        container.arrange(Rect{.origin = {}, .size = measured});
        return measured;
    }

    [[nodiscard]] std::array<float, 3> xs() const {
        return {a->bounds().origin.x, b->bounds().origin.x, c->bounds().origin.x};
    }

    [[nodiscard]] std::array<float, 3> ys() const {
        return {a->bounds().origin.y, b->bounds().origin.y, c->bounds().origin.y};
    }

    [[nodiscard]] std::array<float, 3> heights() const {
        return {a->bounds().size.height, b->bounds().size.height, c->bounds().size.height};
    }
};

LinearLayoutOptions spaced(MainAxisAlignment mainAxis,
                           CrossAxisAlignment crossAxis = CrossAxisAlignment::Start) {
    return LinearLayoutOptions{.spacing = 10.0F, .mainAxis = mainAxis, .crossAxis = crossAxis};
}

using Floats = std::array<float, 3>;

TEST(LinearLayout, AC1_RowWrapsChildren) {
    Line<Row> line{spaced(MainAxisAlignment::Start)};

    EXPECT_EQ(line.layOut(loose(500.0F, 100.0F)), dims(140.0F, 40.0F));
    EXPECT_EQ(line.a->bounds(), rect(0.0F, 0.0F, 50.0F, 30.0F));
    EXPECT_EQ(line.b->bounds(), rect(60.0F, 0.0F, 40.0F, 20.0F));
    EXPECT_EQ(line.c->bounds(), rect(110.0F, 0.0F, 30.0F, 40.0F));
}

TEST(LinearLayout, AC2_ColumnWrapsChildren) {
    Line<Column> line{spaced(MainAxisAlignment::Start)};

    EXPECT_EQ(line.layOut(loose(500.0F, 500.0F)), dims(50.0F, 110.0F));
    EXPECT_EQ(line.a->bounds(), rect(0.0F, 0.0F, 50.0F, 30.0F));
    EXPECT_EQ(line.b->bounds(), rect(0.0F, 40.0F, 40.0F, 20.0F));
    EXPECT_EQ(line.c->bounds(), rect(0.0F, 70.0F, 30.0F, 40.0F));
}

TEST(LinearLayout, AC3_MainAxisAlignment) {
    const auto xsFor = [](MainAxisAlignment alignment) {
        Line<Row> line{spaced(alignment)};
        EXPECT_EQ(line.layOut(tight(290.0F, 40.0F)), dims(290.0F, 40.0F));
        return line.xs();
    };

    EXPECT_EQ(xsFor(MainAxisAlignment::Start), (Floats{0.0F, 60.0F, 110.0F}));
    EXPECT_EQ(xsFor(MainAxisAlignment::Center), (Floats{75.0F, 135.0F, 185.0F}));
    EXPECT_EQ(xsFor(MainAxisAlignment::End), (Floats{150.0F, 210.0F, 260.0F}));
    EXPECT_EQ(xsFor(MainAxisAlignment::SpaceBetween), (Floats{0.0F, 135.0F, 260.0F}));
    EXPECT_EQ(xsFor(MainAxisAlignment::SpaceAround), (Floats{25.0F, 135.0F, 235.0F}));
    EXPECT_EQ(xsFor(MainAxisAlignment::SpaceEvenly), (Floats{37.5F, 135.0F, 222.5F}));
}

TEST(LinearLayout, AC4_CrossAxisAlignment) {
    const auto lineFor = [](CrossAxisAlignment alignment) {
        auto line = std::make_unique<Line<Row>>(spaced(MainAxisAlignment::Start, alignment));
        EXPECT_EQ(line->layOut(tight(140.0F, 60.0F)), dims(140.0F, 60.0F));
        return line;
    };

    const auto start = lineFor(CrossAxisAlignment::Start);
    EXPECT_EQ(start->ys(), (Floats{0.0F, 0.0F, 0.0F}));
    EXPECT_EQ(start->heights(), (Floats{30.0F, 20.0F, 40.0F}));
    EXPECT_EQ(lineFor(CrossAxisAlignment::Center)->ys(), (Floats{15.0F, 20.0F, 10.0F}));
    EXPECT_EQ(lineFor(CrossAxisAlignment::End)->ys(), (Floats{30.0F, 40.0F, 20.0F}));
    const auto stretch = lineFor(CrossAxisAlignment::Stretch);
    EXPECT_EQ(stretch->ys(), (Floats{0.0F, 0.0F, 0.0F}));
    EXPECT_EQ(stretch->heights(), (Floats{60.0F, 60.0F, 60.0F}));
}

TEST(LinearLayout, AC5_WeightsShareLeftoverSpace) {
    Row row;
    const Leaf& a = addLeaf(row, dims(50.0F, 30.0F));
    const Leaf& b = addLeaf(row, dims(10.0F, 10.0F), ModifierChain{}.then(weight(1.0F)));
    const Leaf& c = addLeaf(row, dims(10.0F, 10.0F), ModifierChain{}.then(weight(2.0F)));

    const Size measured = row.measure(tight(290.0F, 50.0F));
    EXPECT_EQ(measured, dims(290.0F, 50.0F));
    row.arrange(Rect{.origin = {}, .size = measured});
    EXPECT_EQ(a.bounds(), rect(0.0F, 0.0F, 50.0F, 30.0F));
    EXPECT_EQ(b.bounds(), rect(50.0F, 0.0F, 80.0F, 10.0F));
    EXPECT_EQ(c.bounds(), rect(130.0F, 0.0F, 160.0F, 10.0F));
}

TEST(LinearLayout, AC6_WeightsAreParentData) {
    Row row;
    const Leaf& a = addLeaf(row, dims(50.0F, 30.0F));
    const Leaf& b = addLeaf(row, dims(10.0F, 10.0F), ModifierChain{}.then(weight(1.0F)));
    const Leaf& c =
        addLeaf(row, dims(10.0F, 10.0F), ModifierChain{}.then(weight(2.0F)).then(weight(5.0F)));

    EXPECT_EQ(row.measure(BoxConstraints::unbounded()), dims(70.0F, 30.0F));
    row.arrange(rect(0.0F, 0.0F, 70.0F, 30.0F));
    EXPECT_EQ(b.bounds().size.width, 10.0F);
    EXPECT_EQ(c.bounds().size.width, 10.0F);

    Leaf alone{dims(50.0F, 30.0F)};
    ASSERT_TRUE(alone.setModifiers(ModifierChain{}.then(weight(3.0F))).has_value());
    EXPECT_EQ(alone.measure(loose(100.0F, 100.0F)), dims(50.0F, 30.0F));

    const auto* found = c.modifiers().find<Weight>();
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->value(), 2.0F);
    EXPECT_EQ(a.modifiers().find<Weight>(), nullptr);
    EXPECT_EQ(c.modifiers().typeIds(),
              (std::vector<std::string_view>{"coil.weight", "coil.weight"}));
    EXPECT_TRUE(c.modifiers().layoutStages().empty());
}

TEST(LinearLayout, AC7_InvalidInputsAreRejected) {
    for (const float bad : {0.0F, -1.0F, kNaN, kUnbounded}) {
        const ModifierResult result = weight(bad);
        ASSERT_FALSE(result.has_value()) << "weight(" << bad << ") should fail";
        EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument);
    }

    Row row;
    ASSERT_TRUE(row.setOptions(LinearLayoutOptions{.spacing = 5.0F}).has_value());
    for (const float bad : {-1.0F, kNaN, kUnbounded}) {
        const Expected<void> result = row.setOptions(LinearLayoutOptions{.spacing = bad});
        ASSERT_FALSE(result.has_value()) << "spacing " << bad << " should fail";
        EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument);
        EXPECT_EQ(row.options().spacing, 5.0F);
    }

    static_cast<void>(addLeaf(row, dims(10.0F, 10.0F)));
    const Expected<void> added = row.add(nullptr);
    ASSERT_FALSE(added.has_value());
    EXPECT_EQ(added.error().code(), ErrorCode::InvalidArgument);
    EXPECT_EQ(row.childCount(), 1U);
}

TEST(LinearLayout, AC8_OverflowingChildren) {
    const auto xsFor = [](MainAxisAlignment alignment) {
        Line<Row> line{spaced(alignment)};
        EXPECT_EQ(line.layOut(tight(100.0F, 40.0F)), dims(100.0F, 40.0F));
        return line.xs();
    };

    EXPECT_EQ(xsFor(MainAxisAlignment::Start), (Floats{0.0F, 60.0F, 110.0F}));
    EXPECT_EQ(xsFor(MainAxisAlignment::Center), (Floats{-20.0F, 40.0F, 90.0F}));
    EXPECT_EQ(xsFor(MainAxisAlignment::SpaceBetween), (Floats{0.0F, 60.0F, 110.0F}));
}

} // namespace
