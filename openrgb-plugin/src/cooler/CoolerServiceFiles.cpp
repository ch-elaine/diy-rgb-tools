#include "CoolerServiceFiles.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <unistd.h>
#include "Hidraw.h"

namespace
{
    constexpr int MaximumChannel    = 255;
    constexpr int MaximumBrightness = 100;
}

bool CoolerServiceFiles::IsInstalled()
{
    return access(UnitPath, F_OK) == 0 && access(ProgramPath, X_OK) == 0;
}

bool CoolerServiceFiles::SettingsAreWritable()
{
    return access(SettingsPath, W_OK) == 0;
}

std::string CoolerServiceFiles::UnitText()
{
    return std::string("[Unit]\n")
         + "Description=DIY RGB Tools: AX120 Digital display and G203 lighting after boot or wake\n"
         + "DefaultDependencies=no\n"
         + "After=systemd-udevd.service\n"
         + "Conflicts=shutdown.target\n"
         + "Before=shutdown.target\n"
         + "\n"
         + "[Service]\n"
         + "ExecStart=" + ProgramPath + " run --settings " + SettingsPath + "\n"
         + "Restart=on-failure\n"
         + "RestartSec=2\n"
         + "DevicePolicy=closed\n"
         + "DeviceAllow=char-hidraw rw\n"
         + "CapabilityBoundingSet=\n"
         + "NoNewPrivileges=yes\n"
         + "ProtectSystem=strict\n"
         + "ProtectHome=yes\n"
         + "ProtectKernelTunables=yes\n"
         + "ProtectKernelModules=yes\n"
         + "ProtectControlGroups=yes\n"
         + "RestrictAddressFamilies=AF_UNIX\n"
         + "RestrictNamespaces=yes\n"
         + "LockPersonality=yes\n"
         + "MemoryDenyWriteExecute=yes\n"
         + "SystemCallArchitectures=native\n"
         + "\n"
         + "[Install]\n"
         + "WantedBy=sysinit.target\n";
}

void CoolerServiceFiles::WriteSettings(const std::string& path, const Ax120PluginSettings& settings)
{
    nlohmann::json settingsJson = settings.display.ToJson();
    settingsJson["brightness"]  = settings.lighting.brightness;
    settingsJson["colours"]     = nlohmann::json::array();

    for(RGBColor color : settings.lighting.colors)
    {
        settingsJson["colours"].push_back({ RGBGetRValue(color), RGBGetGValue(color), RGBGetBValue(color) });
    }

    std::ofstream file(path, std::ios::trunc);
    file << settingsJson.dump(2) << '\n';
    file.close();

    if(!file)
    {
        throw DeviceError("writing " + path + " failed");
    }
}

Ax120PluginSettings CoolerServiceFiles::ReadSettings(const std::string& path)
{
    std::ifstream file(path);

    if(!file)
    {
        throw DeviceError("cannot read " + path);
    }

    const std::string   text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const nlohmann::json settingsJson = nlohmann::json::parse(text, nullptr, false);

    if(!settingsJson.is_object() || !settingsJson.contains("colours") || !settingsJson["colours"].is_array()
       || settingsJson["colours"].size() != Ax120LedCount)
    {
        throw DeviceError(path + " is not a cooler settings file with " + std::to_string(Ax120LedCount) + " colours");
    }

    Ax120PluginSettings settings;
    settings.display = Ax120DisplayOptions::FromJson(settingsJson);

    if(settingsJson.contains("brightness") && settingsJson["brightness"].is_number_integer())
    {
        settings.lighting.brightness = static_cast<unsigned int>(std::clamp(settingsJson["brightness"].get<int>(), 0, MaximumBrightness));
    }

    for(std::size_t ledIndex = 0; ledIndex < Ax120LedCount; ledIndex++)
    {
        const nlohmann::json& colour = settingsJson["colours"][ledIndex];

        if(!colour.is_array() || colour.size() != 3 || !colour[0].is_number_integer() || !colour[1].is_number_integer()
           || !colour[2].is_number_integer())
        {
            throw DeviceError(path + " has a malformed colour at position " + std::to_string(ledIndex + 1));
        }

        const unsigned int red   = static_cast<unsigned int>(std::clamp(colour[0].get<int>(), 0, MaximumChannel));
        const unsigned int green = static_cast<unsigned int>(std::clamp(colour[1].get<int>(), 0, MaximumChannel));
        const unsigned int blue  = static_cast<unsigned int>(std::clamp(colour[2].get<int>(), 0, MaximumChannel));
        settings.lighting.colors[ledIndex] = ToRGBColor(red, green, blue);
    }

    return settings;
}
