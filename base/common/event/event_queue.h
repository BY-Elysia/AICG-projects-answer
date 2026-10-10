#pragma once

#include <functional>
#include <mutex>
#include <queue>
#include <utility>

class EventQueue {
public:
    using EventHandler = std::function<void()>;

public:
    void push(EventHandler const& eventHandler) {
        std::lock_guard<std::mutex> lockGuard{m_mutex};
        m_queue.push(eventHandler);
    }

    void push(EventHandler&& eventHandler) {
        std::lock_guard<std::mutex> lockGuard{m_mutex};
        m_queue.push(std::move(eventHandler));
    }

    void processEvents() {
        std::queue<EventHandler> pendingQueue;
        /* swap all elements of queue to a pending queue */ {
            std::lock_guard<std::mutex> lockGuard{m_mutex};
            pendingQueue.swap(m_queue);
        }

        while (!pendingQueue.empty()) {
            auto eventHandler{std::move(pendingQueue.front())};
            pendingQueue.pop();
            eventHandler();
        }
    }

private:
    std::mutex m_mutex;
    std::queue<EventHandler> m_queue;
};
