#include <coil/render/draw_list.hpp>

#include <utility>

namespace coil::render {

void DrawList::append(DrawCommand command) {
    m_commands.push_back(std::move(command));
    m_commands.pop_back();
}

} // namespace coil::render
