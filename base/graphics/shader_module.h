#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "graphics/rhi_resource.h"

namespace gfx {
class ShaderModule : public RHIResource {
public:
    enum class Stage {
        // traditional geometry pipeline
        Vertex,
        TessControl,
        TessEvaluation,
        Geometry,
        Fragment,
        // compute pipeline
        Compute,
        // Mesh shading pipeline
        Task,
        Mesh,
        // raytracing pipeline
        RayGen,
        RayIntersection,
        RayAnyHit,
        RayClosestHit,
        RayMiss,
        RayCallable,
    };

public:
    ShaderModule(std::string const& code, Stage stage);

    ShaderModule(ShaderModule&& rhs) noexcept = default;

    ~ShaderModule();

    ShaderModule& operator=(ShaderModule&& rhs) noexcept;

private:
    Stage m_stage;
};
} // namespace gfx
