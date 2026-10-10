#pragma once

#include <cstdint>

#include <common/event/event.h>
#include <utility>

class WindowEvent : public Event {
public:
    Category getCategoryBitmask() const noexcept override final {
        return Category::Window;
    }
};

class WindowResizeEvent final : public WindowEvent {
public:
    WindowResizeEvent(uint32_t width, uint32_t height)
        : m_extent{width, height} {}

    uint32_t getWidth() const noexcept {
        return m_extent.first;
    }

    uint32_t getHeight() const noexcept {
        return m_extent.second;
    }

    std::pair<uint32_t, uint32_t> getExtent() const noexcept {
        return m_extent;
    }

    Type getType() const noexcept override {
        return Event::Type::WindowResize;
    };

    std::string getInfo() const override {
        return "WindowResizeEvent: " + std::to_string(m_extent.first) + ", " + std::to_string(m_extent.second);
    }

private:
    std::pair<uint32_t, uint32_t> m_extent;
};

class WindowFramebufferResizeEvent final : public WindowEvent {
public:
    WindowFramebufferResizeEvent(uint32_t width, uint32_t height)
        : m_extent{width, height} {}

    uint32_t getWidth() const noexcept {
        return m_extent.first;
    }

    uint32_t getHeight() const noexcept {
        return m_extent.second;
    }

    std::pair<uint32_t, uint32_t> getExtent() const noexcept {
        return m_extent;
    }

    Type getType() const noexcept override {
        return Event::Type::WindowFramebufferResize;
    };

    std::string getInfo() const override {
        return "WindowFramebufferResizeEvent: " + std::to_string(m_extent.first) + ", " +
               std::to_string(m_extent.second);
    }

private:
    std::pair<uint32_t, uint32_t> m_extent;
};

class WindowCloseEvent final : public WindowEvent {
public:
    Type getType() const noexcept override {
        return Event::Type::WindowClose;
    };

    std::string getInfo() const override {
        return "WindowCloseEvent";
    }
};

class WindowIconifyEvent final : public WindowEvent {
public:
    WindowIconifyEvent(bool iconified)
        : m_iconified(iconified) {}

    bool iconified() const noexcept {
        return m_iconified;
    }

    Type getType() const noexcept override {
        return Event::Type::WindowIconify;
    };

    std::string getInfo() const override {
        return "WindowIconifyEvent: " + std::to_string(m_iconified);
    }

private:
    bool m_iconified;
};
