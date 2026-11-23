#pragma once

#include <array>
#include <vector>
#include "Ax120SegmentLayout.h"
#include "Ax120Settings.h"
#include "HardwareSensors.h"

class Ax120Frame
{
public:
    static constexpr std::array<DisplayedMetric, 4> RotationOrder =
    {
        DisplayedMetric::CpuTemperature,
        DisplayedMetric::CpuUsage,
        DisplayedMetric::GpuTemperature,
        DisplayedMetric::GpuUsage,
    };

    static constexpr std::array<DisplayedMetric, 2> TemperatureRotationOrder =
    {
        DisplayedMetric::CpuTemperature,
        DisplayedMetric::GpuTemperature,
    };

    static SegmentMask                  Mask(const Ax120DisplayOptions& displayOptions, const SensorReadings& sensorReadings,
                                             std::size_t rotationPhase);
    static std::vector<unsigned char>   Payload(const Ax120Lighting& lighting, const SegmentMask& mask);

private:
    static std::optional<double>        ReadingFor(DisplayedMetric metric, const SensorReadings& sensorReadings);
};
