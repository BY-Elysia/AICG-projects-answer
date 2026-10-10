#include <common/application.h>
#include <iostream>
#include <common/event/event_dispatcher.h>

Application::Application(Config const& config)
    : m_window(config.windowConfig)
    , m_input(m_window) {
    if (!config.assetRootDir.empty()) {
        std::filesystem::current_path(config.assetRootDir);
    }

    m_window.setEventCallback([this](Event& event) { onEvent(event); });
}

void Application::run() {
    while (m_running) {
        m_input.forwardKeyStates();
        m_input.forwardMouseStates();

        m_window.pollEvents();
        m_eventQueue.processEvents();

        if (!m_minimized) {
            renderFrame();
            m_window.swapBuffers();
        }

        m_deltaTime = static_cast<float>(m_clock.tock());
        m_clock.tick();
    }
}

void Application::close() {
    m_running = false;
    m_window.close();
}

void Application::resetClock() noexcept {
    m_clock.tick();
}

void Application::onEvent(Event& event) {
#ifndef NDEBUG
    std::cout << event.getInfo() << std::endl;
#endif

    if (event.isInCategory(Event::Category::Input)) {
        m_input.processEvent(event);
    }
    else if (event.isInCategory(Event::Category::Window)) {
        EventDispatcher dispatcher{event};
        dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent& e) { return onWindowClose(e); });
        dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) { return onWindowResize(e); });
        dispatcher.dispatch<WindowIconifyEvent>([this](WindowIconifyEvent& e) { return onWindowIconify(e); });
    }

    handleEvent(event);
}

bool Application::onWindowResize(WindowResizeEvent& event) {
    auto const& [width, height]{event.getExtent()};
    if (width == 0 || height == 0) {
        return false;
    }

    return false;
}

bool Application::onWindowIconify(WindowIconifyEvent& event) {
    m_minimized = event.iconified();

    return false;
}

bool Application::onWindowClose(WindowCloseEvent& event) {
    close();

    return false;
}
