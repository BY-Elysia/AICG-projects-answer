#pragma once

#include <cstdint>

namespace gfx {
struct Viewport {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};
    float minDepth{0.0f};
    float maxDepth{1.0f};

    Viewport() = default;

    Viewport(uint32_t width, uint32_t height)
        : x(0)
        , y(0)
        , width(width)
        , height(height) {}

    Viewport(int x, int y, uint32_t width, uint32_t height)
        : x(x)
        , y(y)
        , width(width)
        , height(height) {}

    Viewport(float x, float y, float width, float height, float minDepth, float maxDepth)
        : x(x)
        , y(y)
        , width(width)
        , height(height)
        , minDepth{minDepth}
        , maxDepth{maxDepth} {}

    bool operator==(Viewport const& rhs) const noexcept {
        return x == rhs.x && y == rhs.y && width == rhs.width && height == rhs.height && minDepth == rhs.minDepth &&
               maxDepth == rhs.maxDepth;
    }

    bool operator!=(Viewport const& rhs) const noexcept {
        return !((*this) == rhs);
    }
};
} // namespace gfx