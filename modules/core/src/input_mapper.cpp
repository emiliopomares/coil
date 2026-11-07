#include <coil/core/input_mapper.hpp>
#include <coil/core/intent.hpp>
#include <coil/platform/input_source.hpp>
#include <coil/platform/window_system.hpp>

#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace coil::core {

namespace {

using platform::Key;

/// Whether holding the key repeats the action (SPEC-0028 rule 4).
bool repeats(IntentAction action) {
    return action != IntentAction::Activate && action != IntentAction::Cancel;
}

Intent toIntent(IntentAction action, platform::WindowId window) {
    const auto navigate = [window](NavigationDirection direction) {
        return Intent{Navigate{.window = window, .direction = direction}};
    };
    switch (action) {
    case IntentAction::Activate:
        return Activate{.window = window};
    case IntentAction::Cancel:
        return Cancel{.window = window};
    case IntentAction::NavigateUp:
        return navigate(NavigationDirection::Up);
    case IntentAction::NavigateDown:
        return navigate(NavigationDirection::Down);
    case IntentAction::NavigateLeft:
        return navigate(NavigationDirection::Left);
    case IntentAction::NavigateRight:
        return navigate(NavigationDirection::Right);
    case IntentAction::NavigateNext:
        return navigate(NavigationDirection::Next);
    case IntentAction::NavigatePrevious:
        return navigate(NavigationDirection::Previous);
    }
    return Cancel{.window = window}; // Not reached: every action is handled above.
}

} // namespace

Bindings Bindings::defaults() {
    Bindings bindings;
    bindings.bind(KeyChord{.key = Key::Enter}, IntentAction::Activate);
    bindings.bind(KeyChord{.key = Key::Space}, IntentAction::Activate);
    bindings.bind(KeyChord{.key = Key::Escape}, IntentAction::Cancel);
    bindings.bind(KeyChord{.key = Key::Tab}, IntentAction::NavigateNext);
    bindings.bind(KeyChord{.key = Key::Tab, .modifiers = {.shift = true}},
                  IntentAction::NavigatePrevious);
    bindings.bind(KeyChord{.key = Key::Up}, IntentAction::NavigateUp);
    bindings.bind(KeyChord{.key = Key::Down}, IntentAction::NavigateDown);
    bindings.bind(KeyChord{.key = Key::Left}, IntentAction::NavigateLeft);
    bindings.bind(KeyChord{.key = Key::Right}, IntentAction::NavigateRight);
    return bindings;
}

void Bindings::bind(KeyChord chord, IntentAction action) {
    m_actions.insert_or_assign(chord, action);
}

void Bindings::unbind(KeyChord chord) {
    m_actions.erase(chord);
}

std::optional<IntentAction> Bindings::find(KeyChord chord) const {
    const auto found = m_actions.find(chord);
    if (found == m_actions.end()) {
        return std::nullopt;
    }
    return found->second;
}

InputMapper::InputMapper(Bindings bindings)
    : m_bindings(std::move(bindings)) {}

const Bindings& InputMapper::bindings() const noexcept {
    return m_bindings;
}

void InputMapper::setBindings(Bindings bindings) {
    m_bindings = std::move(bindings);
}

std::vector<Intent> InputMapper::map(const platform::RawInputEvent& event) const {
    std::vector<Intent> intents;
    if (const auto* key = std::get_if<platform::KeyPressed>(&event)) {
        // Exact chords only; releases give nothing (rule 3).
        const auto action = m_bindings.find(KeyChord{.key = key->key, .modifiers = key->modifiers});
        if (action && (!key->repeat || repeats(*action))) {
            intents.push_back(toIntent(*action, key->window));
        }
    } else if (const auto* moved = std::get_if<platform::PointerMoved>(&event)) {
        intents.emplace_back(PointerMove{
            .window = moved->window, .pointer = moved->pointer, .position = moved->position});
    } else if (const auto* pressed = std::get_if<platform::PointerPressed>(&event)) {
        intents.emplace_back(PointerDown{.window = pressed->window,
                                         .pointer = pressed->pointer,
                                         .button = pressed->button,
                                         .position = pressed->position});
    } else if (const auto* released = std::get_if<platform::PointerReleased>(&event)) {
        intents.emplace_back(PointerUp{.window = released->window,
                                       .pointer = released->pointer,
                                       .button = released->button,
                                       .position = released->position});
    } else if (const auto* scrolled = std::get_if<platform::PointerScrolled>(&event)) {
        intents.emplace_back(Scroll{.window = scrolled->window,
                                    .position = scrolled->position,
                                    .deltaX = scrolled->deltaX,
                                    .deltaY = scrolled->deltaY});
    } else if (const auto* text = std::get_if<platform::TextEntered>(&event)) {
        intents.emplace_back(TextInput{.window = text->window, .text = text->text});
    }
    return intents;
}

} // namespace coil::core
