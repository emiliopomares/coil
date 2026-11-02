#pragma once

#include <coil/foundation/error.hpp>
#include <coil/foundation/geometry.hpp>
#include <coil/layout/box_constraints.hpp>
#include <coil/layout/layout_element.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace coil::layout {

/// How children are spread along the main axis when there is free space (or overflow).
enum class MainAxisAlignment : std::uint8_t {
    Start,
    Center,
    End,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly
};
/// How each child is placed on the cross axis.
enum class CrossAxisAlignment : std::uint8_t { Start, Center, End, Stretch };

/// Settings shared by Row and Column.
struct LinearLayoutOptions {
    float spacing = 0.0F; ///< Between neighbouring children, in DIPs.
    MainAxisAlignment mainAxis = MainAxisAlignment::Start;
    CrossAxisAlignment crossAxis = CrossAxisAlignment::Start;
};

namespace detail {

enum class Axis : std::uint8_t { Horizontal, Vertical };

/// The flex algorithm shared by Row and Column (SPEC-0018 rules 2-7), for one main axis.
class LinearLayout {
public:
    explicit LinearLayout(Axis axis) noexcept
        : m_axis(axis) {}

    [[nodiscard]] Expected<void> setOptions(LinearLayoutOptions options);

    [[nodiscard]] const LinearLayoutOptions& options() const noexcept { return m_options; }

    [[nodiscard]] Expected<void> add(std::unique_ptr<LayoutElement> child);

    [[nodiscard]] std::size_t childCount() const noexcept { return m_children.size(); }

    [[nodiscard]] Size measure(const BoxConstraints& constraints);
    void arrange(Rect contentBounds);

private:
    Axis m_axis;
    LinearLayoutOptions m_options;
    std::vector<std::unique_ptr<LayoutElement>> m_children;
};

} // namespace detail

/// Children left to right, with spacing, alignment and weights (SPEC-0018).
class Row final : public LayoutElement {
public:
    Row() = default;

    /// Fails with InvalidArgument (options unchanged) unless spacing is finite and >= 0.
    [[nodiscard]] Expected<void> setOptions(LinearLayoutOptions options) {
        return m_layout.setOptions(options);
    }

    [[nodiscard]] const LinearLayoutOptions& options() const noexcept { return m_layout.options(); }

    /// Appends a child, which the row owns. Fails with InvalidArgument for a null child.
    [[nodiscard]] Expected<void> add(std::unique_ptr<LayoutElement> child) {
        return m_layout.add(std::move(child));
    }

    [[nodiscard]] std::size_t childCount() const noexcept { return m_layout.childCount(); }

protected:
    [[nodiscard]] Size measureContent(const BoxConstraints& constraints) override {
        return m_layout.measure(constraints);
    }

    void arrangeContent(Rect contentBounds) override { m_layout.arrange(contentBounds); }

private:
    detail::LinearLayout m_layout{detail::Axis::Horizontal};
};

/// Children top to bottom: Row with the axes swapped.
class Column final : public LayoutElement {
public:
    Column() = default;

    /// Fails with InvalidArgument (options unchanged) unless spacing is finite and >= 0.
    [[nodiscard]] Expected<void> setOptions(LinearLayoutOptions options) {
        return m_layout.setOptions(options);
    }

    [[nodiscard]] const LinearLayoutOptions& options() const noexcept { return m_layout.options(); }

    /// Appends a child, which the column owns. Fails with InvalidArgument for a null child.
    [[nodiscard]] Expected<void> add(std::unique_ptr<LayoutElement> child) {
        return m_layout.add(std::move(child));
    }

    [[nodiscard]] std::size_t childCount() const noexcept { return m_layout.childCount(); }

protected:
    [[nodiscard]] Size measureContent(const BoxConstraints& constraints) override {
        return m_layout.measure(constraints);
    }

    void arrangeContent(Rect contentBounds) override { m_layout.arrange(contentBounds); }

private:
    detail::LinearLayout m_layout{detail::Axis::Vertical};
};

} // namespace coil::layout
