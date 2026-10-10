#pragma once

#include <common/enum.h>

#include <string>
#include <type_traits>

class Event {
public:
    enum class Type {
        None,
        WindowClose,
        WindowResize,
        WindowFramebufferResize,
        WindowIconify,
        KeyPress,
        KeyRelease,
        KeyType,
        MouseMove,
        MouseScroll,
        MouseButtonPress,
        MouseButtonRelease,
        MouseButtonHold,
    };

    enum class Category {
        None = makeBitmaskBit(0u),
        Input = makeBitmaskBit(1u),
        Window = makeBitmaskBit(2u),
        Keyboard = makeBitmaskBit(3u),
        Mouse = makeBitmaskBit(4u),
        MouseButton = makeBitmaskBit(5u),
    };

public:
    bool isHandled{false};

public:
    virtual ~Event() = default;

    bool isInCategory(Category category) const noexcept {
        using T = std::underlying_type_t<Category>;
        return static_cast<T>(getCategoryBitmask()) & static_cast<T>(category);
    }

    virtual Type getType() const noexcept = 0;

    virtual Category getCategoryBitmask() const noexcept = 0;

    virtual std::string getInfo() const = 0;
};
