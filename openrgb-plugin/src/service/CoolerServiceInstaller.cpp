#include "CoolerServiceInstaller.h"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "CoolerServiceFiles.h"
#include "Hidraw.h"

namespace
{
    constexpr const char*   SystemctlPath    = "/usr/bin/systemctl";
    constexpr const char*   OwnProgramPath   = "/proc/self/exe";
    constexpr mode_t        ProgramMode      = 0755;
    constexpr mode_t        ReadableFileMode = 0644;
    constexpr mode_t        DirectoryMode    = 0755;

    void CreateDirectory(const std::string& path)
    {
        std::filesystem::create_directories(path);
        chmod(path.c_str(), DirectoryMode);
    }

    void RemoveIfEmpty(const std::string& path)
    {
        std::error_code error;
        if(std::filesystem::is_empty(path, error) && !error)
        {
            std::filesystem::remove(path, error);
        }
    }
}

void CoolerServiceInstaller::Install(const std::string& ownerName, const std::string& stagedSettingsPath)
{
    RequireRoot();

    const passwd* owner = getpwnam(ownerName.c_str());

    if(owner == nullptr)
    {
        throw DeviceError("unknown user " + ownerName);
    }

    const Ax120PluginSettings settings = CoolerServiceFiles::ReadSettings(stagedSettingsPath);

    CreateDirectory(CoolerServiceFiles::ProgramDirectory);
    CreateDirectory(CoolerServiceFiles::SettingsDirectory);
    InstallProgram();

    CoolerServiceFiles::WriteSettings(CoolerServiceFiles::SettingsPath, settings);

    if(chown(CoolerServiceFiles::SettingsPath, owner->pw_uid, owner->pw_gid) != 0 || chmod(CoolerServiceFiles::SettingsPath, ReadableFileMode) != 0)
    {
        throw DeviceError(std::string("cannot hand ") + CoolerServiceFiles::SettingsPath + " to " + ownerName + ": " + std::strerror(errno));
    }

    WriteFile(CoolerServiceFiles::UnitPath, CoolerServiceFiles::UnitText(), ReadableFileMode);

    if(!RunSystemctl({ "daemon-reload" }) || !RunSystemctl({ "enable", CoolerServiceFiles::ServiceName })
       || !RunSystemctl({ "restart", CoolerServiceFiles::ServiceName }))
    {
        throw DeviceError("systemctl could not enable and start the service");
    }
}

void CoolerServiceInstaller::Remove()
{
    RequireRoot();
    RunSystemctl({ "disable", "--now", CoolerServiceFiles::ServiceName });

    std::error_code error;
    std::filesystem::remove(CoolerServiceFiles::UnitPath, error);
    std::filesystem::remove(CoolerServiceFiles::ProgramPath, error);
    std::filesystem::remove(CoolerServiceFiles::SettingsPath, error);
    RemoveIfEmpty(CoolerServiceFiles::ProgramDirectory);
    RemoveIfEmpty(CoolerServiceFiles::SettingsDirectory);
    RunSystemctl({ "daemon-reload" });
}

void CoolerServiceInstaller::RequireRoot()
{
    if(geteuid() != 0)
    {
        throw DeviceError("installing or removing the service needs root; the plugin runs this through pkexec");
    }
}

void CoolerServiceInstaller::InstallProgram()
{
    const std::string temporaryPath = std::string(CoolerServiceFiles::ProgramPath) + ".new";
    std::filesystem::copy_file(OwnProgramPath, temporaryPath, std::filesystem::copy_options::overwrite_existing);

    if(chmod(temporaryPath.c_str(), ProgramMode) != 0 || rename(temporaryPath.c_str(), CoolerServiceFiles::ProgramPath) != 0)
    {
        throw DeviceError(std::string("cannot install ") + CoolerServiceFiles::ProgramPath + ": " + std::strerror(errno));
    }
}

void CoolerServiceInstaller::WriteFile(const std::string& path, const std::string& content, mode_t mode)
{
    std::ofstream file(path, std::ios::trunc);
    file << content;
    file.close();

    if(!file || chmod(path.c_str(), mode) != 0)
    {
        throw DeviceError("cannot write " + path);
    }
}

bool CoolerServiceInstaller::RunSystemctl(const std::vector<std::string>& arguments)
{
    std::vector<char*> argumentPointers{ const_cast<char*>(SystemctlPath) };

    for(const std::string& argument : arguments)
    {
        argumentPointers.push_back(const_cast<char*>(argument.c_str()));
    }
    argumentPointers.push_back(nullptr);

    const pid_t child = fork();

    if(child < 0)
    {
        return false;
    }

    if(child == 0)
    {
        execv(SystemctlPath, argumentPointers.data());
        _exit(127);
    }

    int status = 0;
    while(waitpid(child, &status, 0) < 0 && errno == EINTR)
    {
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
