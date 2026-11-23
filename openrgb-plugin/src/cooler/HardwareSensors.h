#pragma once

#include <filesystem>
#include <optional>

class SensorReadings
{
public:
    std::optional<double> cpuTemperature;
    std::optional<double> cpuUsage;
    std::optional<double> gpuTemperature;
    std::optional<double> gpuUsage;
};

class HardwareSensors
{
public:
    HardwareSensors();

    SensorReadings Read();

private:
    std::filesystem::path   cpuTemperaturePath;
    std::filesystem::path   gpuTemperaturePath;
    std::filesystem::path   gpuUsagePath;
    unsigned long long      previousCpuIdleTime  = 0;
    unsigned long long      previousCpuTotalTime = 0;

    void                            FindCpuTemperature();
    void                            FindGpuSensors();
    std::optional<double>           ReadCpuUsage();

    static std::optional<double>    ReadNumber(const std::filesystem::path& path);
    static std::string              ReadText(const std::filesystem::path& path);
};
