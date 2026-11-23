#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "Ax120Settings.h"

class CoolerServiceFiles
{
public:
    static constexpr const char*    ServiceName       = "diy-rgb-tools-cooler.service";
    static constexpr const char*    ProgramDirectory  = "/usr/local/lib/diy-rgb-tools";
    static constexpr const char*    ProgramPath       = "/usr/local/lib/diy-rgb-tools/diy-rgb-tools-cooler";
    static constexpr const char*    SettingsDirectory = "/etc/diy-rgb-tools";
    static constexpr const char*    SettingsPath      = "/etc/diy-rgb-tools/cooler.json";
    static constexpr const char*    UnitPath          = "/etc/systemd/system/diy-rgb-tools-cooler.service";

    static bool                     IsInstalled();
    static bool                     SettingsAreWritable();
    static std::string              UnitText();
    static void                     WriteSettings(const std::string& path, const Ax120PluginSettings& settings);
    static Ax120PluginSettings      ReadSettings(const std::string& path);
};
