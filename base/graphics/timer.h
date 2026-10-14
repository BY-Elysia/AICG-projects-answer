#pragma once

#include "graphics/rhi_resource.h"

namespace gfx {

class Timer : public RHIResource {
public:
    Timer();

    Timer(Timer&& rhs) noexcept = default;

    ~Timer();

    Timer& operator=(Timer&& rhs) noexcept;

    void tick() const;

    void tock() const;

    bool available() const;

    float getElapsedTime() const;
};

} // namespace gfx
