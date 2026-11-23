#include "MouseLightingKeeper.h"

#include <cstdio>
#include <ctime>
#include "G203Protocol.h"
#include "Hidraw.h"

namespace
{
    constexpr std::chrono::seconds SettleTime{ 3 };
    constexpr std::chrono::seconds SleepThreshold{ 5 };
}

MouseLightingKeeper::MouseLightingKeeper()
    : lastBootTime(BootTime()),
      lastSteadyTime(Clock::now())
{
}

void MouseLightingKeeper::Tick()
{
    if(WokeFromSleep())
    {
        redrawPending = true;
        foundAt.reset();
    }

    if(!redrawPending)
    {
        return;
    }

    const std::optional<std::string> path = G203Protocol::FindPath();
    const Clock::time_point          now  = Clock::now();

    if(!path.has_value())
    {
        foundAt.reset();
        return;
    }

    if(!foundAt.has_value())
    {
        foundAt = now;
    }

    if(now - *foundAt >= SettleTime)
    {
        redrawPending = false;
        foundAt.reset();
        Redraw(*path);
    }
}

bool MouseLightingKeeper::WokeFromSleep()
{
    const std::chrono::nanoseconds bootTime   = BootTime();
    const Clock::time_point        steadyTime = Clock::now();
    const std::chrono::nanoseconds asleepFor  = (bootTime - lastBootTime) - (steadyTime - lastSteadyTime);

    lastBootTime   = bootTime;
    lastSteadyTime = steadyTime;
    return asleepFor > SleepThreshold;
}

void MouseLightingKeeper::Redraw(const std::string& devicePath)
{
    try
    {
        HidrawHandle handle(devicePath);
        G203Protocol mouse(handle, HidppProtocol::ServiceSoftwareId);

        if(mouse.RedrawOnboardLighting())
        {
            Log("redrew the G203's stored lighting");
        }
        else
        {
            Log("left the G203 alone: a program is controlling its lighting");
        }
    }
    catch(const DeviceError& error)
    {
        Log(std::string("could not redraw the G203's lighting: ") + error.what());
    }
}

std::chrono::nanoseconds MouseLightingKeeper::BootTime()
{
    timespec bootTime{};
    clock_gettime(CLOCK_BOOTTIME, &bootTime);
    return std::chrono::seconds(bootTime.tv_sec) + std::chrono::nanoseconds(bootTime.tv_nsec);
}

void MouseLightingKeeper::Log(const std::string& message)
{
    std::printf("%s\n", message.c_str());
    std::fflush(stdout);
}
