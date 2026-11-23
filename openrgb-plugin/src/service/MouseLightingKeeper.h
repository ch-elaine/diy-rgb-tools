#pragma once

#include <chrono>
#include <optional>
#include <string>

class MouseLightingKeeper
{
public:
    MouseLightingKeeper();

    void    Tick();

private:
    using Clock = std::chrono::steady_clock;

    bool                                redrawPending = true;
    std::optional<Clock::time_point>    foundAt;
    std::chrono::nanoseconds            lastBootTime;
    Clock::time_point                   lastSteadyTime;

    bool                                WokeFromSleep();
    void                                Redraw(const std::string& devicePath);
    static std::chrono::nanoseconds     BootTime();
    static void                         Log(const std::string& message);
};
