#pragma once

#include <array>
#include <chrono>
#include <functional>
#include <vector>
#include "Hidraw.h"
#include "RGBControllerInterface.h"

class PolychromeLayout
{
public:
    static constexpr std::size_t    ZoneCount   = 8;
    static constexpr unsigned char  ZoneAbsent  = 0x1E;

    std::array<unsigned char, ZoneCount>    ledCounts{};
    unsigned char                           availableMask = 0;
    unsigned char                           rgbSwapMask   = 0;

    std::vector<unsigned char>              PresentZones() const;
};

class PolychromeZoneState
{
public:
    unsigned char   effect = 0;
    RGBColor        color  = 0;
    unsigned char   speed  = 0;

    bool            operator==(const PolychromeZoneState& other) const;
};

class PolychromeProtocol
{
public:
    static constexpr unsigned char  SetZoneCommand        = 0x10;
    static constexpr unsigned char  ReadZoneCommand       = 0x11;
    static constexpr unsigned char  CommitCommand         = 0x12;
    static constexpr unsigned char  ReadHeaderCommand     = 0x14;
    static constexpr unsigned char  WriteHeaderCommand    = 0x15;
    static constexpr unsigned char  AvailableZonesHeader  = 0x01;
    static constexpr unsigned char  LedCountsHeader       = 0x02;
    static constexpr unsigned char  RgbSwapHeader         = 0x03;
    static constexpr unsigned char  DirectEffect          = 0x0F;

    explicit PolychromeProtocol(HidTransport& transportReference);

    PolychromeLayout        ReadLayout();
    ByteVector              ReadHeader(unsigned char header);
    void                    WriteHeader(unsigned char header, const ByteVector& data);
    PolychromeZoneState     ReadZone(unsigned char zone, bool swapped);
    void                    SetZone(unsigned char zone, const PolychromeZoneState& zoneState, bool swapped);
    void                    StreamDirect(const ByteVector& wireColours);
    void                    Commit();

    static ByteVector       WireColour(RGBColor color, bool swapped);
    static RGBColor         ColourFromWire(const unsigned char* wireColour, bool swapped);

private:
    static constexpr std::size_t    ReportSize = 64;

    HidTransport&           transport;

    ByteVector              Request(unsigned char command, unsigned char argument, const ByteVector& body,
                                    const std::function<bool(const ByteVector&)>& accept,
                                    std::chrono::milliseconds timeout);
    ByteVector              Exchange(const ByteVector& report, const std::function<bool(const ByteVector&)>& accept,
                                     std::chrono::milliseconds timeout, unsigned char command);
};
