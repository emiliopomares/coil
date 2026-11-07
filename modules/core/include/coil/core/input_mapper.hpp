#pragma once

#include <coil/core/intent.hpp>
#include <coil/platform/input_source.hpp>

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <tuple>
#include <vector>

namespace coil::core {

/// A key with an exact set of modifiers, e.g. Shift+Tab.
struct KeyChord {
    platform::Key key = platform::Key::A;
    platform::KeyModifiers modifiers{};

    friend constexpr bool operator==(const KeyChord&, const KeyChord&) = default;

    friend constexpr auto operator<=>(const KeyChord& a, const KeyChord& b) {
        const auto tie = [](const KeyChord& chord) {
            return std::tuple{chord.key, chord.modifiers.shift, chord.modifiers.control,
                              chord.modifiers.alt, chord.modifiers.super};
        };
        return tie(a) <=> tie(b);
    }
};

/// What a key binding produces.
enum class IntentAction : std::uint8_t {
    Activate,
    Cancel,
    NavigateUp,
    NavigateDown,
    NavigateLeft,
    NavigateRight,
    NavigateNext,
    NavigatePrevious,
};

/// Key bindings: each chord maps to at most one action (SPEC-0028). A value type.
class Bindings {
public:
    /// The default bindings (SPEC-0028 rule 2): Enter and Space activate, Escape cancels, Tab and
    /// Shift+Tab move through the focus order, arrows navigate.
    [[nodiscard]] static Bindings defaults();

    /// Binds `chord` to `action`, replacing any action it had.
    void bind(KeyChord chord, IntentAction action);
    /// Removes the chord's binding, if any.
    void unbind(KeyChord chord);
    /// The chord's action, if bound.
    [[nodiscard]] std::optional<IntentAction> find(KeyChord chord) const;

    friend bool operator==(const Bindings&, const Bindings&) = default;

private:
    std::map<KeyChord, IntentAction> m_actions;
};

/// Turns raw input into intents (ADR-0007 layer 2). Stateless apart from its bindings.
class InputMapper {
public:
    explicit InputMapper(Bindings bindings = Bindings::defaults());

    [[nodiscard]] const Bindings& bindings() const noexcept;
    /// Replaces all bindings; affects later map() calls only.
    void setBindings(Bindings bindings);

    /// The intents for one raw event: none or one (SPEC-0028 rules 3-7).
    [[nodiscard]] std::vector<Intent> map(const platform::RawInputEvent& event) const;

private:
    Bindings m_bindings;
};

} // namespace coil::core
