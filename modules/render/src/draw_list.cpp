#include <coil/render/draw_list.hpp>

#include <utility>

namespace coil::render {

void DrawList::append(DrawCommand command) {
    m_commands.push_back(std::move(command));
}

} // namespace coil::render
