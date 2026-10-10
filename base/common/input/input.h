#pragma once

#include <array>
#include <cstddef>
#include <utility>

#include "input_mapping.h"

class Window;
class Event;
class Application;

class Input {
public:
    explicit Input(Window& window);

    ~Input() = default;

    bool isKeyPressed(KeyCode keycode) const noexcept;

    bool isKeyHeld(KeyCode keycode) const noexcept;

    bool isKeyReleased(KeyCode keycode) const noexcept;

    bool isKeyDown(KeyCode keycode) const noexcept;

    bool isMouseButtonPressed(MouseButton button) const noexcept;

    bool isMouseButtonHeld(MouseButton button) const noexcept;

    bool isMouseButtonDown(MouseButton button) const noexcept;

    float getMouseX() const noexcept;

    float getMouseY() const noexcept;

    std::pair<float, float> getMousePosition() const noexcept;

    void setMousePosition(float x, float y);

    float getMouseScrollX() const noexcept;

    float getMouseScrollY() const noexcept;

    std::pair<float, float> getMouseScroll() const noexcept;

    CursorMode getCursorMode() const;

    void setCursorMode(CursorMode mode);

    void printKeyStates() const;

    void printMouseButtonStates() const;

private:
    friend class Application;

    void processEvent(Event& event) noexcept;

    Window& m_window;
    std::pair<float, float> m_mousePosition;
    std::pair<float, float> m_mouseScroll;
    std::array<KeyState, 349> m_keyStates;
    std::array<MouseButtonState, 4> m_mouseButtonStates;

private:
    void updateKeyState(KeyCode keycode, KeyState state) noexcept;

    void forwardKeyStates() noexcept;

    void updateMouseButtonState(MouseButton button, MouseButtonState state) noexcept;

    void forwardMouseStates() noexcept;
};
