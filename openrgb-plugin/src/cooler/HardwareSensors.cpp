#include "HardwareSensors.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <string>
#include <system_error>

namespace
{
    const std::filesystem::path HwmonDirectory = "/sys/class/hwmon";
    const std::filesystem::path DrmDirectory   = "/sys/class/drm";
    const std::filesystem::path ProcStatPath   = "/proc/stat";

    const std::map<std::string, std::string> PreferredCpuTemperatureLabels =
    {
        { "k10temp",  "Tctl"         },
        { "zenpower", "Tdie"         },
        { "coretemp", "Package id 0" },
    };

    constexpr double MillidegreesPerDegree = 1000.0;
}

HardwareSensors::HardwareSensors()
{
    FindCpuTemperature();
    FindGpuSensors();
    ReadCpuUsage();
}

SensorReadings HardwareSensors::Read()
{
    SensorReadings readings;

    if(const auto cpuMillidegrees = ReadNumber(cpuTemperaturePath))
    {
        readings.cpuTemperature = *cpuMillidegrees / MillidegreesPerDegree;
    }

    if(const auto gpuMillidegrees = ReadNumber(gpuTemperaturePath))
    {
        readings.gpuTemperature = *gpuMillidegrees / MillidegreesPerDegree;
    }

    readings.cpuUsage = ReadCpuUsage();
    readings.gpuUsage = ReadNumber(gpuUsagePath);
    return readings;
}

void HardwareSensors::FindCpuTemperature()
{
    std::error_code error;

    for(const auto& hwmonEntry : std::filesystem::directory_iterator(HwmonDirectory, error))
    {
        const std::string driverName = ReadText(hwmonEntry.path() / "name");
        const auto        preferred  = PreferredCpuTemperatureLabels.find(driverName);

        if(preferred == PreferredCpuTemperatureLabels.end())
        {
            continue;
        }

        cpuTemperaturePath = hwmonEntry.path() / "temp1_input";

        for(int sensorNumber = 1; sensorNumber <= 32; sensorNumber++)
        {
            const std::string sensorPrefix = "temp" + std::to_string(sensorNumber);

            if(ReadText(hwmonEntry.path() / (sensorPrefix + "_label")) == preferred->second)
            {
                cpuTemperaturePath = hwmonEntry.path() / (sensorPrefix + "_input");
                break;
            }
        }

        return;
    }
}

void HardwareSensors::FindGpuSensors()
{
    std::error_code    error;
    unsigned long long largestVideoMemory = 0;

    for(const auto& drmEntry : std::filesystem::directory_iterator(DrmDirectory, error))
    {
        const std::string entryName = drmEntry.path().filename().string();

        if(entryName.rfind("card", 0) != 0 || entryName.find('-') != std::string::npos)
        {
            continue;
        }

        const std::filesystem::path deviceDirectory = drmEntry.path() / "device";
        const std::filesystem::path busyPath        = deviceDirectory / "gpu_busy_percent";

        if(!std::filesystem::exists(busyPath, error))
        {
            continue;
        }

        const unsigned long long videoMemory = static_cast<unsigned long long>(ReadNumber(deviceDirectory / "mem_info_vram_total").value_or(0));

        if(!gpuUsagePath.empty() && videoMemory <= largestVideoMemory)
        {
            continue;
        }

        largestVideoMemory = videoMemory;
        gpuUsagePath       = busyPath;
        gpuTemperaturePath.clear();

        for(const auto& hwmonEntry : std::filesystem::directory_iterator(deviceDirectory / "hwmon", error))
        {
            gpuTemperaturePath = hwmonEntry.path() / "temp1_input";
            break;
        }
    }
}

std::optional<double> HardwareSensors::ReadCpuUsage()
{
    std::ifstream statFile(ProcStatPath);
    std::string   cpuLabel;
    std::array<unsigned long long, 8> times{};

    statFile >> cpuLabel;

    for(unsigned long long& time : times)
    {
        statFile >> time;
    }

    if(!statFile || cpuLabel != "cpu")
    {
        return std::nullopt;
    }

    const unsigned long long idleTime  = times[3] + times[4];
    unsigned long long       totalTime = 0;

    for(unsigned long long time : times)
    {
        totalTime += time;
    }

    const unsigned long long idleDelta  = idleTime - previousCpuIdleTime;
    const unsigned long long totalDelta = totalTime - previousCpuTotalTime;
    const bool               hasPrevious = previousCpuTotalTime != 0;

    previousCpuIdleTime  = idleTime;
    previousCpuTotalTime = totalTime;

    if(!hasPrevious || totalDelta == 0)
    {
        return std::nullopt;
    }

    return std::clamp(100.0 * static_cast<double>(totalDelta - idleDelta) / static_cast<double>(totalDelta), 0.0, 100.0);
}

std::optional<double> HardwareSensors::ReadNumber(const std::filesystem::path& path)
{
    if(path.empty())
    {
        return std::nullopt;
    }

    std::ifstream file(path);
    double        number = 0.0;

    if(!(file >> number))
    {
        return std::nullopt;
    }

    return number;
}

std::string HardwareSensors::ReadText(const std::filesystem::path& path)
{
    std::ifstream file(path);
    std::string   text;
    std::getline(file, text);
    return text;
}
