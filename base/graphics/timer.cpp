#include "graphics/timer.h"

#include <utility>
#include "graphics/gl_utility.h"

namespace gfx {

Timer::Timer() {
    glGenQueries(1, &m_handle);
}

Timer::~Timer() {
    if (isValid()) {
        glDeleteQueries(1, &m_handle);
    }
}

Timer& Timer::operator=(Timer&& rhs) noexcept {
    if (this != &rhs) {
        if (isValid()) {
            glDeleteQueries(1, &m_handle);
        }
        RHIResource::operator=(std::move(rhs));
    }
    return *this;
}

void Timer::tick() const {
    glBeginQuery(GL_TIME_ELAPSED, m_handle);
}

void Timer::tock() const {
    glEndQuery(GL_TIME_ELAPSED);
}

bool Timer::available() const {
    GLint ready = GL_FALSE;
    glGetQueryObjectiv(m_handle, GL_QUERY_RESULT_AVAILABLE, &ready);
    return ready == GL_TRUE;
}

float Timer::getElapsedTime() const {
    GLuint64 nanoseconds = 0;
    glGetQueryObjectui64v(m_handle, GL_QUERY_RESULT, &nanoseconds);
    return static_cast<float>(nanoseconds) / 1'000'000'000.0f;
}

} // namespace gfx
