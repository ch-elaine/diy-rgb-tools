#include "CoolerServiceProgram.h"

#include <cstdio>
#include <exception>
#include "CoolerServiceDaemon.h"
#include "CoolerServiceFiles.h"
#include "CoolerServiceInstaller.h"

int CoolerServiceProgram::Run(int argumentCount, char** argumentValues)
{
    arguments.assign(argumentValues + 1, argumentValues + argumentCount);

    if(arguments.empty())
    {
        return PrintUsage();
    }

    try
    {
        const std::string& command = arguments.front();

        if(command == "run")
        {
            CoolerServiceDaemon(OptionValue("--settings").value_or(CoolerServiceFiles::SettingsPath)).Run();
        }

        if(command == "install")
        {
            const std::optional<std::string> owner    = OptionValue("--owner");
            const std::optional<std::string> settings = OptionValue("--settings");

            if(!owner.has_value() || !settings.has_value())
            {
                return PrintUsage();
            }

            CoolerServiceInstaller::Install(*owner, *settings);
            std::printf("installed and started %s\n", CoolerServiceFiles::ServiceName);
            return 0;
        }

        if(command == "remove")
        {
            CoolerServiceInstaller::Remove();
            std::printf("removed %s\n", CoolerServiceFiles::ServiceName);
            return 0;
        }
    }
    catch(const std::exception& error)
    {
        std::fprintf(stderr, "error: %s\n", error.what());
        return 1;
    }

    return PrintUsage();
}

std::optional<std::string> CoolerServiceProgram::OptionValue(const std::string& name) const
{
    for(std::size_t index = 1; index + 1 < arguments.size(); index++)
    {
        if(arguments[index] == name)
        {
            return arguments[index + 1];
        }
    }
    return std::nullopt;
}

int CoolerServiceProgram::PrintUsage()
{
    std::fprintf(stderr,
                 "usage: diy-rgb-tools-cooler run [--settings FILE]\n"
                 "       diy-rgb-tools-cooler install --owner USER --settings FILE\n"
                 "       diy-rgb-tools-cooler remove\n"
                 "Drives the Thermalright AX120 Digital display while OpenRGB is closed. The DIY RGB Tools\n"
                 "plugin installs, updates and removes it from its Cooler tab.\n");
    return 2;
}
