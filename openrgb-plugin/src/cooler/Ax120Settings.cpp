#include "Ax120Settings.h"

#include <algorithm>
#include <map>
#include <string>

namespace
{
    const std::map<DisplayContent, std::string> displayContentNames =
    {
        { DisplayContent::Rotate,         "rotate"          },
        { DisplayContent::CpuTemperature, "cpu_temperature" },
        { DisplayContent::CpuUsage,       "cpu_usage"       },
        { DisplayContent::GpuTemperature, "gpu_temperature" },
        { DisplayContent::GpuUsage,       "gpu_usage"       },
        { DisplayContent::AllSegments,    "all_segments"    },
        { DisplayContent::RotateTemperatures, "rotate_temperatures" },
    };

    DisplayContent DisplayContentFromName(const std::string& name)
    {
        for(const auto& [content, contentName] : displayContentNames)
        {
            if(contentName == name)
            {
                return content;
            }
        }
        return DisplayContent::Rotate;
    }
}

Ax120Lighting::Ax120Lighting()
{
    colors.fill(ToRGBColor(255, 255, 255));
}

bool Ax120DisplayOptions::IsRotating() const
{
    return content == DisplayContent::Rotate || content == DisplayContent::RotateTemperatures;
}

bool Ax120DisplayOptions::operator==(const Ax120DisplayOptions& other) const
{
    return content == other.content && temperatureUnit == other.temperatureUnit && rotationSeconds == other.rotationSeconds;
}

bool Ax120DisplayOptions::operator!=(const Ax120DisplayOptions& other) const
{
    return !(*this == other);
}

nlohmann::json Ax120DisplayOptions::ToJson() const
{
    nlohmann::json optionsJson;
    optionsJson["display_content"]  = displayContentNames.at(content);
    optionsJson["temperature_unit"] = temperatureUnit == TemperatureUnit::Fahrenheit ? "fahrenheit" : "celsius";
    optionsJson["rotation_seconds"] = rotationSeconds;
    return optionsJson;
}

Ax120DisplayOptions Ax120DisplayOptions::FromJson(const nlohmann::json& optionsJson)
{
    Ax120DisplayOptions options;

    if(!optionsJson.is_object())
    {
        return options;
    }

    if(optionsJson.contains("display_content") && optionsJson["display_content"].is_string())
    {
        options.content = DisplayContentFromName(optionsJson["display_content"].get<std::string>());
    }

    if(optionsJson.contains("temperature_unit") && optionsJson["temperature_unit"].is_string())
    {
        options.temperatureUnit = optionsJson["temperature_unit"].get<std::string>() == "fahrenheit"
                                ? TemperatureUnit::Fahrenheit
                                : TemperatureUnit::Celsius;
    }

    if(optionsJson.contains("rotation_seconds") && optionsJson["rotation_seconds"].is_number_integer())
    {
        options.rotationSeconds = std::clamp(optionsJson["rotation_seconds"].get<int>(), 1, 60);
    }

    return options;
}

nlohmann::json Ax120PluginSettings::ToJson() const
{
    nlohmann::json settingsJson = display.ToJson();
    settingsJson["brightness"]  = lighting.brightness;
    settingsJson["colors"]      = std::vector<RGBColor>(lighting.colors.begin(), lighting.colors.end());
    return settingsJson;
}

Ax120PluginSettings Ax120PluginSettings::FromJson(const nlohmann::json& settingsJson)
{
    Ax120PluginSettings settings;

    if(!settingsJson.is_object())
    {
        return settings;
    }

    settings.display = Ax120DisplayOptions::FromJson(settingsJson);

    if(settingsJson.contains("brightness") && settingsJson["brightness"].is_number_unsigned())
    {
        settings.lighting.brightness = std::min(settingsJson["brightness"].get<unsigned int>(), 100u);
    }

    if(settingsJson.contains("colors") && settingsJson["colors"].is_array() && settingsJson["colors"].size() == Ax120LedCount)
    {
        for(std::size_t ledIndex = 0; ledIndex < Ax120LedCount; ledIndex++)
        {
            if(settingsJson["colors"][ledIndex].is_number_unsigned())
            {
                settings.lighting.colors[ledIndex] = settingsJson["colors"][ledIndex].get<RGBColor>();
            }
        }
    }

    return settings;
}
