#pragma once

#include <utility>

#include <common/event/event.h>
#include <common/input/input_mapping.h>

class MouseEvent : public Event {
public:
    Category getCategoryBitmask() const noexcept override final {
        using T = std::underlying_type_t<Category>;

        T bitmask = {static_cast<T>(Category::Mouse) | static_cast<T>(Category::Input)};
        return static_cast<Category>(bitmask);
    }
};

class MouseButtonEvent : public Event {
public:
    Category getCategoryBitmask() const noexcept override final {
        using T = std::underlying_type_t<Category>;

        T bitmask = {static_cast<T>(Category::MouseButton) | static_cast<T>(Category::Input)};
        return static_cast<Category>(bitmask);
    }
};

class MouseMoveEvent final : public MouseEvent {
public:
    MouseMoveEvent(float x, float y)
        : m_position{x, y} {};

    float getX() const noexcept {
        return m_position.first;
    }

    float getY() const noexcept {
        return m_position.second;
    }

    std::pair<float, float> getPosition() const noexcept {
        return m_position;
    }

    Type getType() const noexcept override {
        return Event::Type::MouseMove;
    };

    std::string getInfo() const override {
        auto const x = m_position.first;
        auto const y = m_position.second;
        return "MouseMoveEvent: " + std::to_string(x) + " " + std::to_string(y);
    }

private:
    std::pair<float, float> m_position;
};

class MouseScrollEvent final : public MouseEvent {
public:
    MouseScrollEvent(float scrollX, float scrollY)
        : m_scroll{scrollX, scrollY} {}

    float getScrollX() const noexcept {
        return m_scroll.first;
    }

    float getScrollY() const noexcept {
        return m_scroll.second;
    }

    std::pair<float, float> getScroll() const noexcept {
        return m_scroll;
    }

    Type getType() const noexcept override {
        return Event::Type::MouseScroll;
    };

    std::string getInfo() const override {
        auto const x = m_scroll.first;
        auto const y = m_scroll.second;
        return "MouseScrollEvent: " + std::to_string(x) + " " + std::to_string(y);
    }

private:
    std::pair<float, float> m_scroll;
};

class MouseButtonPressEvent final : public MouseButtonEvent {
public:
    MouseButtonPressEvent(MouseButton button)
        : m_button(button) {}

    MouseButton getButton() const noexcept {
        return m_button;
    }

    Type getType() const noexcept override {
        return Event::Type::MouseButtonPress;
    };

    std::string getInfo() const override {
        return "MouseButtonPressEvent: " + std::to_string(static_cast<int>(m_button));
    }

private:
    MouseButton m_button;
};

class MouseButtonReleaseEvent final : public MouseButtonEvent {
public:
    MouseButtonReleaseEvent(MouseButton button)
        : m_button(button) {}

    MouseButton getButton() const noexcept {
        return m_button;
    }

    Type getType() const noexcept override {
        return Event::Type::MouseButtonRelease;
    };

    std::string getInfo() const override {
        return "MouseButtonReleaseEvent: " + std::to_string(static_cast<int>(m_button));
    }

private:
    MouseButton m_button;
};

class MouseButtonHoldEvent final : public MouseButtonEvent {
public:
    MouseButtonHoldEvent(MouseButton button)
        : m_button(button) {}

    MouseButton getButton() const noexcept {
        return m_button;
    }

    Type getType() const noexcept override {
        return Event::Type::MouseButtonHold;
    };

    std::string getInfo() const override {
        return "MouseButtonHoldEvent: " + std::to_string(static_cast<int>(m_button));
    }

private:
    MouseButton m_button;
};
