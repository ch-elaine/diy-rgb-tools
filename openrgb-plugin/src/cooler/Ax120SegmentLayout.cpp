#include "Ax120SegmentLayout.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    constexpr std::array<std::size_t, 2> AccentLeds      = { 0, 1 };
    constexpr std::array<std::size_t, 2> CpuLabelLeds    = { 2, 3 };
    constexpr std::array<std::size_t, 2> GpuLabelLeds    = { 4, 5 };
    constexpr std::size_t                CelsiusLed      = 6;
    constexpr std::size_t                FahrenheitLed   = 7;
    constexpr std::size_t                PercentLed      = 8;
    constexpr std::size_t                FirstDigitLed   = 9;
    constexpr std::size_t                SegmentsPerDigit = 7;
    constexpr std::size_t                DigitCount      = 3;

    constexpr std::array<unsigned char, 10> DigitPatterns =
    {
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
    };
    constexpr unsigned char DashPattern = 0x40;

    constexpr std::array<const char*, SegmentsPerDigit> SegmentNames =
    {
        "Top", "Upper Right", "Lower Right", "Bottom", "Lower Left", "Upper Left", "Middle"
    };

    constexpr std::array<std::pair<unsigned int, unsigned int>, SegmentsPerDigit> SegmentMatrixCells =
    {{
        { 0, 1 }, { 1, 2 }, { 3, 2 }, { 4, 1 }, { 3, 0 }, { 1, 0 }, { 2, 1 }
    }};

    constexpr unsigned int DigitMatrixHeight      = 5;
    constexpr unsigned int DigitMatrixColumnPitch = 4;
    constexpr unsigned int DigitMatrixWidth       = DigitMatrixColumnPitch * DigitCount - 1;
    constexpr unsigned int EmptyMatrixCell        = 0xFFFFFFFF;

    std::vector<std::string> BuildLedNames()
    {
        std::vector<std::string> names =
        {
            "Accent Left", "Accent Right",
            "CPU Label Left", "CPU Label Right",
            "GPU Label Left", "GPU Label Right",
            "Celsius", "Fahrenheit", "Percent",
        };

        for(std::size_t digitPosition = 0; digitPosition < DigitCount; digitPosition++)
        {
            for(const char* segmentName : SegmentNames)
            {
                names.push_back("Digit " + std::to_string(digitPosition + 1) + " " + segmentName);
            }
        }

        return names;
    }

    std::vector<unsigned int> BuildDigitMatrixMap()
    {
        std::vector<unsigned int> matrixMap(DigitMatrixHeight * DigitMatrixWidth, EmptyMatrixCell);

        for(unsigned int digitPosition = 0; digitPosition < DigitCount; digitPosition++)
        {
            for(unsigned int segmentIndex = 0; segmentIndex < SegmentsPerDigit; segmentIndex++)
            {
                const auto [row, column] = SegmentMatrixCells[segmentIndex];
                const unsigned int cell  = row * DigitMatrixWidth + digitPosition * DigitMatrixColumnPitch + column;
                matrixMap[cell]          = digitPosition * SegmentsPerDigit + segmentIndex;
            }
        }

        return matrixMap;
    }

    std::vector<Ax120ZoneLayout> BuildZones()
    {
        return
        {
            { "Accent", ZONE_TYPE_LINEAR, 0, 2, 0, 0, {} },
            { "Labels", ZONE_TYPE_LINEAR, 2, 7, 0, 0, {} },
            { "Digits", ZONE_TYPE_MATRIX, static_cast<unsigned int>(FirstDigitLed), static_cast<unsigned int>(SegmentsPerDigit * DigitCount),
              DigitMatrixHeight, DigitMatrixWidth, BuildDigitMatrixMap() },
        };
    }
}

const std::vector<std::string>& Ax120SegmentLayout::LedNames()
{
    static const std::vector<std::string> ledNames = BuildLedNames();
    return ledNames;
}

const std::vector<Ax120ZoneLayout>& Ax120SegmentLayout::Zones()
{
    static const std::vector<Ax120ZoneLayout> zones = BuildZones();
    return zones;
}

SegmentMask Ax120SegmentLayout::AllSegments()
{
    SegmentMask mask;
    mask.fill(true);
    return mask;
}

SegmentMask Ax120SegmentLayout::ForMetric(DisplayedMetric metric, std::optional<double> value, TemperatureUnit temperatureUnit)
{
    SegmentMask mask{};

    for(std::size_t ledIndex : AccentLeds)
    {
        mask[ledIndex] = true;
    }

    const bool isCpuMetric         = metric == DisplayedMetric::CpuTemperature || metric == DisplayedMetric::CpuUsage;
    const bool isTemperatureMetric = metric == DisplayedMetric::CpuTemperature || metric == DisplayedMetric::GpuTemperature;

    for(std::size_t ledIndex : isCpuMetric ? CpuLabelLeds : GpuLabelLeds)
    {
        mask[ledIndex] = true;
    }

    if(isTemperatureMetric)
    {
        mask[temperatureUnit == TemperatureUnit::Fahrenheit ? FahrenheitLed : CelsiusLed] = true;
    }
    else
    {
        mask[PercentLed] = true;
    }

    if(!value.has_value())
    {
        DrawDashes(mask);
        return mask;
    }

    double displayedValue = *value;

    if(isTemperatureMetric && temperatureUnit == TemperatureUnit::Fahrenheit)
    {
        displayedValue = displayedValue * 9.0 / 5.0 + 32.0;
    }

    DrawNumber(std::lround(displayedValue), mask);
    return mask;
}

void Ax120SegmentLayout::DrawNumber(long number, SegmentMask& mask)
{
    const long clampedNumber = std::clamp(number, 0L, 999L);
    const int  hundreds      = static_cast<int>(clampedNumber / 100);
    const int  tens          = static_cast<int>(clampedNumber / 10 % 10);
    const int  ones          = static_cast<int>(clampedNumber % 10);

    if(clampedNumber >= 100)
    {
        DrawPattern(0, DigitPatterns[hundreds], mask);
    }

    if(clampedNumber >= 10)
    {
        DrawPattern(1, DigitPatterns[tens], mask);
    }

    DrawPattern(2, DigitPatterns[ones], mask);
}

void Ax120SegmentLayout::DrawDashes(SegmentMask& mask)
{
    for(std::size_t digitPosition = 0; digitPosition < DigitCount; digitPosition++)
    {
        DrawPattern(digitPosition, DashPattern, mask);
    }
}

void Ax120SegmentLayout::DrawPattern(std::size_t digitPosition, unsigned char segmentPattern, SegmentMask& mask)
{
    const std::size_t firstSegmentLed = FirstDigitLed + digitPosition * SegmentsPerDigit;

    for(std::size_t segmentIndex = 0; segmentIndex < SegmentsPerDigit; segmentIndex++)
    {
        if(segmentPattern & (1u << segmentIndex))
        {
            mask[firstSegmentLed + segmentIndex] = true;
        }
    }
}
