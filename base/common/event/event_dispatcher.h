#pragma once

#include <functional>
#include <type_traits>

#include <common/event/event.h>

class EventDispatcher {
public:
    template <typename EventType> using EventHandleFunc = std::function<bool(EventType&)>;

public:
    EventDispatcher(Event& event)
        : m_event(event) {}

    template <typename EventType> bool dispatch(EventHandleFunc<EventType> handleEvent) {
        if (typeid(EventType) != typeid(m_event)) {
            return false;
        }

        m_event.isHandled = handleEvent(static_cast<EventType&>(m_event));
        return true;
    }

private:
    Event& m_event;
};
