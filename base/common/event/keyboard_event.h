#pragma once

#include <cstdint>

#include <common/event/event.h>
#include <common/input/input_mapping.h>

class KeyboardEvent : public Event {
public:
    KeyCode getKeyCode() const noexcept {
        return m_code;
    }

    Category getCategoryBitmask() const noexcept override final {
        using T = std::underlying_type_t<Category>;

        T bitmask = {static_cast<T>(Category::Keyboard) | static_cast<T>(Category::Input)};
        return static_cast<Category>(bitmask);
    }

protected:
    KeyCode m_code;

    KeyboardEvent(KeyCode code)
        : m_code{code} {}
};

class KeyPressEvent final : public KeyboardEvent {
public:
    KeyPressEvent(KeyCode code, bool isRepeated)
        : KeyboardEvent(code)
        , m_isRepeated{isRepeated} {}

    bool isRepeated() const noexcept {
        return m_isRepeated;
    }

    Type getType() const noexcept override {
        return Event::Type::KeyPress;
    };

    std::string getInfo() const override {
        auto info{"KeyPressEvent: " + std::to_string(static_cast<int>(m_code))};
        if (m_isRepeated) {
            info += " repeated";
        }

        return info;
    }

private:
    bool m_isRepeated;
};

class KeyReleaseEvent final : public KeyboardEvent {
public:
    KeyReleaseEvent(KeyCode const keycode)
        : KeyboardEvent(keycode) {}

    Type getType() const noexcept override {
        return Event::Type::KeyRelease;
    };

    std::string getInfo() const override {
        return "KeyReleaseEvent: " + std::to_string(static_cast<int>(m_code));
    }
};

class KeyTypeEvent final : public Event {
public:
    KeyTypeEvent(uint32_t codepoint)
        : m_codepoint(codepoint) {}

    uint32_t getUnicode() const noexcept {
        return m_codepoint;
    }

    Category getCategoryBitmask() const noexcept override final {
        using T = std::underlying_type_t<Category>;

        T bitmask = {static_cast<T>(Category::Keyboard) | static_cast<T>(Category::Input)};
        return static_cast<Category>(bitmask);
    }

    Type getType() const noexcept override {
        return Event::Type::KeyType;
    };

    std::string getInfo() const override {
        // Note: On windows console, to proper output utf8 characters, one need:
        // // 1. Set console code page to UTF-8 so console known how to interpret string data
        // SetConsoleOutputCP(CP_UTF8);
        // // 2. Enable buffering to prevent VS from chopping up UTF-8 byte sequences
        // setvbuf(stdout, nullptr, _IOFBF, 1000);

        return "KeyTypeEvent: " + toUTF8();
    }

    std::string toUTF8() const {
        // we need to translate the unicode(UTF-32) to native character
        // @ref https://www.glfw.org/docs/3.2/input_guide.html
        // @ref https://www.jianshu.com/p/4672f38f6c1e
        std::string s;
        if (m_codepoint <= 0x7F) {
            s += static_cast<char>(m_codepoint & 0xFF);
        }
        else if (m_codepoint <= 0x7FF) {
            s += static_cast<char>(0xC0 | ((m_codepoint >> 6) & 0xFF));
            s += static_cast<char>(0x80 | ((m_codepoint & 0x3F)));
        }
        else if (m_codepoint <= 0xFFFF) {
            s += static_cast<char>(0xE0 | ((m_codepoint >> 12) & 0xFF));
            s += static_cast<char>(0x80 | ((m_codepoint >> 6) & 0x3F));
            s += static_cast<char>(0x80 | (m_codepoint & 0x3F));
        }
        else {
            s += static_cast<char>(0xF0 | ((m_codepoint >> 18) & 0xFF));
            s += static_cast<char>(0x80 | ((m_codepoint >> 12) & 0x3F));
            s += static_cast<char>(0x80 | ((m_codepoint >> 6) & 0x3F));
            s += static_cast<char>(0x80 | (m_codepoint & 0x3F));
        }

        return s;
    }

private:
    // unicode(UTF-32)
    uint32_t m_codepoint;
};
