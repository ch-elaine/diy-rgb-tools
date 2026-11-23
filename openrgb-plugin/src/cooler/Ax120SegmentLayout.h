#pragma once

#include <optional>
#include <string>
#include <vector>
#include "Ax120Settings.h"

using SegmentMask = std::array<bool, Ax120LedCount>;

enum class DisplayedMetric
{
    CpuTemperature,
    CpuUsage,
    GpuTemperature,
    GpuUsage,
};

class Ax120ZoneLayout
{
public:
    std::string                 name;
    zone_type                   type;
    unsigned int                firstLed;
    unsigned int                ledCount;
    unsigned int                matrixHeight;
    unsigned int                matrixWidth;
    std::vector<unsigned int>   matrixMap;
};

class Ax120SegmentLayout
{
public:
    static const std::vector<std::string>&      LedNames();
    static const std::vector<Ax120ZoneLayout>&  Zones();

    static SegmentMask  AllSegments();
    static SegmentMask  ForMetric(DisplayedMetric metric, std::optional<double> value, TemperatureUnit temperatureUnit);

private:
    static void         DrawNumber(long number, SegmentMask& mask);
    static void         DrawDashes(SegmentMask& mask);
    static void         DrawPattern(std::size_t digitPosition, unsigned char segmentPattern, SegmentMask& mask);
};
