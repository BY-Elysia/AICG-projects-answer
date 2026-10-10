#pragma once

#include <common/input/input.h>
#include <common/window.h>
#include <common/event/event.h>
#include <common/event/event_queue.h>
#include <common/event/window_event.h>
#include <common/timer.h>

#include <filesystem>

class Application {
public:
    struct Config {
        Window::Config windowConfig;
        std::filesystem::path assetRootDir;
    };

public:
    explicit Application(Config const& config);

    virtual ~Application() = default;

    void run();

    void close();

protected:
    Window m_window;

    Input m_input;

protected:
    float getDeltaTime() const noexcept {
        return m_deltaTime;
    }

    void resetClock() noexcept;

    virtual void handleEvent(Event& event) = 0;

    virtual void renderFrame() = 0;

private:
    /* application states */
    bool m_running{true};
    bool m_minimized{false};

    /* delta time */
    Timer m_clock;
    float m_deltaTime{0.0};

    /* event queue */
    EventQueue m_eventQueue;

private:
    void onEvent(Event& event);

    bool onWindowResize(WindowResizeEvent& event);

    bool onWindowIconify(WindowIconifyEvent& event);

    bool onWindowClose(WindowCloseEvent& event);
};
