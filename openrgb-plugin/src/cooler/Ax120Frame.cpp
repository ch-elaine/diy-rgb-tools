#include "Ax120Frame.h"

namespace
{
    constexpr double HardwareColorScale = 0.4;
}

SegmentMask Ax120Frame::Mask(const Ax120DisplayOptions& displayOptions, const SensorReadings& sensorReadings, std::size_t rotationPhase)
{
    DisplayedMetric metric = RotationOrder[rotationPhase % RotationOrder.size()];

    switch(displayOptions.content)
    {
        case DisplayContent::AllSegments:    return Ax120SegmentLayout::AllSegments();
        case DisplayContent::CpuTemperature: metric = DisplayedMetric::CpuTemperature; break;
        case DisplayContent::CpuUsage:       metric = DisplayedMetric::CpuUsage;       break;
        case DisplayContent::GpuTemperature: metric = DisplayedMetric::GpuTemperature; break;
        case DisplayContent::GpuUsage:       metric = DisplayedMetric::GpuUsage;       break;
        case DisplayContent::Rotate:                                                   break;
    }

    return Ax120SegmentLayout::ForMetric(metric, ReadingFor(metric, sensorReadings), displayOptions.temperatureUnit);
}

std::vector<unsigned char> Ax120Frame::Payload(const Ax120Lighting& lighting, const SegmentMask& mask)
{
    const double               channelScale = lighting.brightness / 100.0 * HardwareColorScale;
    std::vector<unsigned char> payload;
    payload.reserve(Ax120LedCount * 3);

    for(std::size_t ledIndex = 0; ledIndex < Ax120LedCount; ledIndex++)
    {
        const RGBColor color = mask[ledIndex] ? lighting.colors[ledIndex] : 0;
        payload.push_back(static_cast<unsigned char>(RGBGetRValue(color) * channelScale));
        payload.push_back(static_cast<unsigned char>(RGBGetGValue(color) * channelScale));
        payload.push_back(static_cast<unsigned char>(RGBGetBValue(color) * channelScale));
    }

    return payload;
}

std::optional<double> Ax120Frame::ReadingFor(DisplayedMetric metric, const SensorReadings& sensorReadings)
{
    switch(metric)
    {
        case DisplayedMetric::CpuTemperature: return sensorReadings.cpuTemperature;
        case DisplayedMetric::CpuUsage:       return sensorReadings.cpuUsage;
        case DisplayedMetric::GpuTemperature: return sensorReadings.gpuTemperature;
        case DisplayedMetric::GpuUsage:       return sensorReadings.gpuUsage;
    }
    return std::nullopt;
}
