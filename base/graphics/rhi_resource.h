#pragma once

#include <cstdint>
#include <utility>

namespace gfx {
class RHIResource {
public:
    bool operator==(RHIResource const& rhs) const noexcept {
        return m_handle == rhs.m_handle;
    }

    bool operator!=(RHIResource const& rhs) const noexcept {
        return m_handle != rhs.m_handle;
    }

    uint32_t getNativeHandle() const noexcept {
        return m_handle;
    }

    bool isValid() const noexcept {
        return m_handle != 0;
    }

protected:
    uint32_t m_handle{0};

protected:
    RHIResource() = default;

    RHIResource(RHIResource&& rhs) noexcept
        : m_handle{std::exchange(rhs.m_handle, 0u)} {}

    RHIResource& operator=(RHIResource&& rhs) noexcept {
        if (this != &rhs) {
            m_handle = std::exchange(rhs.m_handle, 0u);
        }

        return *this;
    }
};
} // namespace gfx