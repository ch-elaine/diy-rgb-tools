#include "PolychromeProtocol.h"

#include <algorithm>
#include <cstdio>

namespace
{
    constexpr std::chrono::milliseconds ReplyTimeout{ 1000 };
    constexpr std::chrono::milliseconds CommitTimeout{ 3000 };
    constexpr unsigned char             AllZones             = 0xFF;
    constexpr unsigned char             FirstDirectChunk     = 0xE3;
    constexpr unsigned char             FollowingDirectChunk = 0xE4;
    constexpr unsigned char             DirectChunkMarker    = 0x65;
    constexpr std::size_t               FirstChunkOffset     = 9;
    constexpr std::size_t               FirstChunkCapacity   = 54;
    constexpr std::size_t               FollowingChunkOffset = 5;
    constexpr std::size_t               FollowingChunkCapacity = 57;
    constexpr std::size_t               DirectMarkerOffset   = 64;
    constexpr std::size_t               FirstChunkFlagOffset = 7;
}

std::vector<unsigned char> PolychromeLayout::PresentZones() const
{
    std::vector<unsigned char> zones;

    for(unsigned char zone = 0; zone < ZoneCount; zone++)
    {
        if(ledCounts[zone] != ZoneAbsent && (availableMask >> zone & 1))
        {
            zones.push_back(zone);
        }
    }

    return zones;
}

bool PolychromeZoneState::operator==(const PolychromeZoneState& other) const
{
    return effect == other.effect && color == other.color && speed == other.speed;
}

PolychromeProtocol::PolychromeProtocol(HidTransport& transportReference)
    : transport(transportReference)
{
    transport.Drain();
}

PolychromeLayout PolychromeProtocol::ReadLayout()
{
    PolychromeLayout layout;
    const ByteVector ledCounts = ReadHeader(LedCountsHeader);

    std::copy_n(ledCounts.begin(), PolychromeLayout::ZoneCount, layout.ledCounts.begin());
    layout.availableMask = ReadHeader(AvailableZonesHeader)[0];
    layout.rgbSwapMask   = ReadHeader(RgbSwapHeader)[0];
    return layout;
}

ByteVector PolychromeProtocol::ReadHeader(unsigned char header)
{
    const ByteVector reply = Request(ReadHeaderCommand, header, {},
                                     [header](const ByteVector& candidate) { return candidate[3] == header; }, ReplyTimeout);
    return ByteVector(reply.begin() + 4, reply.end());
}

void PolychromeProtocol::WriteHeader(unsigned char header, const ByteVector& data)
{
    Request(WriteHeaderCommand, header, data, nullptr, ReplyTimeout);
}

PolychromeZoneState PolychromeProtocol::ReadZone(unsigned char zone, bool swapped)
{
    const ByteVector reply = Request(ReadZoneCommand, zone, {},
                                     [zone](const ByteVector& candidate) { return candidate[3] == zone; }, ReplyTimeout);
    PolychromeZoneState zoneState;
    zoneState.effect = reply[4];
    zoneState.color  = ColourFromWire(&reply[5], swapped);
    zoneState.speed  = reply[8];
    return zoneState;
}

void PolychromeProtocol::SetZone(unsigned char zone, const PolychromeZoneState& zoneState, bool swapped)
{
    ByteVector body{ zoneState.effect };
    const ByteVector wireColour = WireColour(zoneState.color, swapped);
    body.insert(body.end(), wireColour.begin(), wireColour.end());
    body.push_back(zoneState.speed);
    body.push_back(0xFF);
    Request(SetZoneCommand, zone, body, nullptr, ReplyTimeout);
}

void PolychromeProtocol::StreamDirect(const ByteVector& wireColours)
{
    std::size_t sent  = 0;
    bool        first = true;

    while(sent < wireColours.size())
    {
        ByteVector report(ReportSize + 1, 0);
        report[1] = SetZoneCommand;
        report[3] = AllZones;
        report[4] = first ? FirstDirectChunk : FollowingDirectChunk;

        if(first)
        {
            report[FirstChunkFlagOffset] = 0xFF;
        }
        report[DirectMarkerOffset] = DirectChunkMarker;

        const std::size_t offset   = first ? FirstChunkOffset : FollowingChunkOffset;
        const std::size_t capacity = first ? FirstChunkCapacity : FollowingChunkCapacity;
        const std::size_t length   = std::min(capacity, wireColours.size() - sent);
        std::copy_n(wireColours.begin() + static_cast<std::ptrdiff_t>(sent), length, report.begin() + static_cast<std::ptrdiff_t>(offset));

        Exchange(report, nullptr, ReplyTimeout, SetZoneCommand);
        sent += length;
        first = false;
    }
}

void PolychromeProtocol::Commit()
{
    Request(CommitCommand, 0, {}, nullptr, CommitTimeout);
}

ByteVector PolychromeProtocol::WireColour(RGBColor color, bool swapped)
{
    const unsigned char red   = static_cast<unsigned char>(RGBGetRValue(color));
    const unsigned char green = static_cast<unsigned char>(RGBGetGValue(color));
    const unsigned char blue  = static_cast<unsigned char>(RGBGetBValue(color));
    return swapped ? ByteVector{ red, green, blue } : ByteVector{ green, red, blue };
}

RGBColor PolychromeProtocol::ColourFromWire(const unsigned char* wireColour, bool swapped)
{
    return swapped ? ToRGBColor(wireColour[0], wireColour[1], wireColour[2])
                   : ToRGBColor(wireColour[1], wireColour[0], wireColour[2]);
}

ByteVector PolychromeProtocol::Request(unsigned char command, unsigned char argument, const ByteVector& body,
                                       const std::function<bool(const ByteVector&)>& accept,
                                       std::chrono::milliseconds timeout)
{
    ByteVector report(ReportSize + 1, 0);
    report[1] = command;
    report[3] = argument;
    std::copy(body.begin(), body.end(), report.begin() + 4);
    return Exchange(report, accept, timeout, command);
}

ByteVector PolychromeProtocol::Exchange(const ByteVector& report, const std::function<bool(const ByteVector&)>& accept,
                                        std::chrono::milliseconds timeout, unsigned char command)
{
    transport.Write(report);
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while(std::chrono::steady_clock::now() < deadline)
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        const std::optional<ByteVector> reply = transport.Read(std::max(remaining, std::chrono::milliseconds(0)));

        if(!reply.has_value())
        {
            break;
        }

        if(reply->size() >= 9 && (!accept || accept(*reply)))
        {
            return *reply;
        }
    }

    char commandText[8];
    std::snprintf(commandText, sizeof(commandText), "0x%02x", command);
    throw DeviceError(std::string("the board did not answer command ") + commandText);
}
