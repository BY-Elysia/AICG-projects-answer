#pragma once

#include <glm/glm.hpp>

namespace gfx {
struct Scissor {
    glm::ivec2 offset;
    glm::uvec2 extent;

    bool operator==(Scissor const& rhs) const noexcept {
        return offset == rhs.offset && extent == rhs.extent;
    }

    bool operator!=(Scissor const& rhs) const noexcept {
        return !((*this) == rhs);
    }
};
} // namespace gfx