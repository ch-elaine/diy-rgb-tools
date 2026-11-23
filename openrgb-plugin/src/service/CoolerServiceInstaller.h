#pragma once

#include <string>
#include <vector>
#include <sys/types.h>

class CoolerServiceInstaller
{
public:
    static void     Install(const std::string& ownerName, const std::string& stagedSettingsPath);
    static void     Remove();

private:
    static void     RequireRoot();
    static void     InstallProgram();
    static void     WriteFile(const std::string& path, const std::string& content, mode_t mode);
    static bool     RunSystemctl(const std::vector<std::string>& arguments);
};
