#pragma once

#include <utility>

struct GLFWwindow;

namespace gfx {
class GraphicsContext {
public:
    GraphicsContext(std::pair<int, int> const& minimumVersion);

    ~GraphicsContext() = default;

    std::pair<int, int> getDriverVersion() const noexcept {
        return m_driverVersion;
    }

    int getMajorVersion() const noexcept {
        return m_driverVersion.first;
    }

    int getMinorVersion() const noexcept {
        return m_driverVersion.second;
    }

    void init(GLFWwindow* window);

    void restoreDepthStates();

    void restoreStencilStates();

private:
    std::pair<int, int> m_driverVersion;

private:
    bool checkVersion(GLFWwindow* window);

    bool registerDebugOutput();
};
} // namespace gfx