#include "CoolerServiceDaemon.h"

#include <cstdio>
#include <thread>
#include "Ax120DisplayLock.h"
#include "Ax120Frame.h"
#include "CoolerServiceFiles.h"
#include "Hidraw.h"

namespace
{
    constexpr std::chrono::seconds TickInterval{ 1 };
    constexpr std::chrono::seconds RetryInterval{ 2 };
}

CoolerServiceDaemon::CoolerServiceDaemon(std::string settingsFilePath)
    : settingsPath(std::move(settingsFilePath)),
      settings(CoolerServiceFiles::ReadSettings(settingsPath)),
      settingsModified(ModifiedTime()),
      nextRotation(Clock::now() + std::chrono::seconds(settings.display.rotationSeconds))
{
}

void CoolerServiceDaemon::Run()
{
    Report("starting", "starting with the settings in " + settingsPath);

    while(true)
    {
        std::this_thread::sleep_for(Tick() ? TickInterval : RetryInterval);
    }
}

bool CoolerServiceDaemon::Tick()
{
    mouseLightingKeeper.Tick();
    ReloadIfChanged();
    const SensorReadings readings = sensors.Read();
    AdvanceRotation();

    Ax120DisplayLock displayLock;

    if(!displayLock.TryAcquire())
    {
        Report("paused", "OpenRGB's DIY RGB Tools plugin is driving the display; paused until it lets go");
        return true;
    }

    if(!device.IsOpen() && !Connect())
    {
        return false;
    }

    const SegmentMask mask = Ax120Frame::Mask(settings.display, readings, rotationPhase);

    if(!device.SendColors(Ax120Frame::Payload(settings.lighting, mask)))
    {
        Report("lost", "lost the display at " + device.Path() + "; retrying");
        device.Close();
        return false;
    }

    Report("driving", "driving the display at " + device.Path());
    return true;
}

bool CoolerServiceDaemon::Connect()
{
    if(!device.Open())
    {
        Report("missing", "display not found (USB 0416:8001 with a hidraw node); retrying");
        return false;
    }

    const HandshakeResult handshakeResult = device.Handshake();

    if(handshakeResult == HandshakeResult::UnsupportedModel)
    {
        Report("unsupported", device.Path() + " reports model byte " + std::to_string(*device.ProductModel())
                              + ", which is not a 30-LED AX120-style display; not driving it");
        device.Close();
        return false;
    }

    Report("connected", "connected to " + device.Path() + (handshakeResult == HandshakeResult::SupportedModel
                        ? " (model byte " + std::to_string(*device.ProductModel()) + ")"
                        : " (no handshake reply, already answered since power-on)"));
    return true;
}

void CoolerServiceDaemon::ReloadIfChanged()
{
    const std::optional<std::filesystem::file_time_type> modified = ModifiedTime();

    if(modified == settingsModified)
    {
        return;
    }

    try
    {
        settings = CoolerServiceFiles::ReadSettings(settingsPath);
    }
    catch(const DeviceError& error)
    {
        Report("unreadable settings", std::string("keeping the previous settings: ") + error.what());
        return;
    }

    settingsModified = modified;
    Report("reloaded", "settings changed; reloaded them");
}

void CoolerServiceDaemon::AdvanceRotation()
{
    if(!settings.display.IsRotating())
    {
        return;
    }

    const Clock::time_point now = Clock::now();

    if(now >= nextRotation)
    {
        rotationPhase = (rotationPhase + 1) % Ax120Frame::RotationOrder.size();
        nextRotation  = now + std::chrono::seconds(settings.display.rotationSeconds);
    }
}

std::optional<std::filesystem::file_time_type> CoolerServiceDaemon::ModifiedTime() const
{
    std::error_code error;
    const std::filesystem::file_time_type modified = std::filesystem::last_write_time(settingsPath, error);

    if(error)
    {
        return std::nullopt;
    }
    return modified;
}

void CoolerServiceDaemon::Report(const std::string& state, const std::string& message)
{
    if(state != reportedState)
    {
        reportedState = state;
        std::printf("%s\n", message.c_str());
        std::fflush(stdout);
    }
}
