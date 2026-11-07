#include <coil/core/input_mapper.hpp>
#include <coil/core/intent.hpp>
#include <coil/platform/input_source.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace coil::core {

Bindings Bindings::defaults() {
    return Bindings{}; // Red stub (SPEC-0028)
}

void Bindings::bind(KeyChord chord, IntentAction action) {
    static_cast<void>(m_actions.size());
    static_cast<void>(chord);
    static_cast<void>(action); // Red stub (SPEC-0028)
}

void Bindings::unbind(KeyChord chord) {
    static_cast<void>(m_actions.size());
    static_cast<void>(chord); // Red stub (SPEC-0028)
}

std::optional<IntentAction> Bindings::find(KeyChord chord) const {
    static_cast<void>(m_actions.size());
    static_cast<void>(chord);
    return std::nullopt; // Red stub (SPEC-0028)
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
    static_cast<void>(m_bindings);
    static_cast<void>(event);
    return {}; // Red stub (SPEC-0028)
}

} // namespace coil::core
