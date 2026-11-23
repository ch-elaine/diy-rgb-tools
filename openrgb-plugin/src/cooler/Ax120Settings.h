#pragma once

#include <array>
#include <nlohmann/json.hpp>
#include "RGBControllerInterface.h"

constexpr std::size_t Ax120LedCount = 30;

using Ax120LedColors = std::array<RGBColor, Ax120LedCount>;

enum class DisplayContent
{
    Rotate,
    CpuTemperature,
    CpuUsage,
    GpuTemperature,
    GpuUsage,
    AllSegments,
};

enum class TemperatureUnit
{
    Celsius,
    Fahrenheit,
};

class Ax120DisplayOptions
{
public:
    DisplayContent  content         = DisplayContent::Rotate;
    TemperatureUnit temperatureUnit = TemperatureUnit::Celsius;
    int             rotationSeconds = 3;

    bool                        operator==(const Ax120DisplayOptions& other) const;
    bool                        operator!=(const Ax120DisplayOptions& other) const;
    nlohmann::json              ToJson() const;
    static Ax120DisplayOptions  FromJson(const nlohmann::json& optionsJson);
};

class Ax120Lighting
{
public:
    Ax120Lighting();

    Ax120LedColors  colors;
    unsigned int    brightness = 100;
};

class Ax120PluginSettings
{
public:
    Ax120DisplayOptions display;
    Ax120Lighting       lighting;

    nlohmann::json              ToJson() const;
    static Ax120PluginSettings  FromJson(const nlohmann::json& settingsJson);
};
