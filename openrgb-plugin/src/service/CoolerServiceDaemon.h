#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include "Ax120HidDevice.h"
#include "Ax120Settings.h"
#include "HardwareSensors.h"
#include "MouseLightingKeeper.h"

class CoolerServiceDaemon
{
public:
    explicit CoolerServiceDaemon(std::string settingsFilePath);

    [[noreturn]] void   Run();

private:
    using Clock = std::chrono::steady_clock;

    std::string                                         settingsPath;
    Ax120PluginSettings                                 settings;
    std::optional<std::filesystem::file_time_type>      settingsModified;
    HardwareSensors                                     sensors;
    MouseLightingKeeper                                 mouseLightingKeeper;
    Ax120HidDevice                                      device;
    std::size_t                                         rotationPhase = 0;
    Clock::time_point                                   nextRotation;
    std::string                                         reportedState;

    bool                                                Tick();
    bool                                                Connect();
    void                                                ReloadIfChanged();
    void                                                AdvanceRotation();
    std::optional<std::filesystem::file_time_type>      ModifiedTime() const;
    void                                                Report(const std::string& state, const std::string& message);
};
