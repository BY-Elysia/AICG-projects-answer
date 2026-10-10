#pragma once

#include <chrono>

struct Timer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

public:
    Timer() = default;

    void tick() {
        m_lastTimestamp = Clock::now();
    }

    double tock() {
        TimePoint now = Clock::now();
        Duration duration = now - m_lastTimestamp;
        m_totalTime += duration;
        m_lastTimestamp = now;

        return toSeconds(duration);
    }

    double getTotalTime() const noexcept {
        return toSeconds(m_totalTime);
    }

private:
    TimePoint m_lastTimestamp{Clock::now()};
    Duration m_totalTime{Duration::zero()};

private:
    static double toSeconds(Duration const& duration) noexcept {
        static_assert(Duration::period::num == 1);
        return 1.0 * duration.count() / Duration::period::den;
    }
};
