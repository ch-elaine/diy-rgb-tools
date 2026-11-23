#include "G203Protocol.h"

#include <algorithm>
#include <cstdio>

namespace
{
    constexpr unsigned char             LongReport          = 0x11;
    constexpr std::size_t               LongReportSize      = 20;
    constexpr unsigned char             DeviceIndex         = 0xFF;
    constexpr unsigned char             SoftwareId          = 0x0D;
    constexpr unsigned char             ErrorMarker         = 0xFF;
    constexpr unsigned char             RootFeatureIndex    = 0x00;
    constexpr unsigned char             GetFeatureFunction  = 0x00;
    constexpr std::chrono::milliseconds ReplyTimeout{ 1000 };

    constexpr unsigned short            RgbEffectsFeature      = 0x8071;
    constexpr unsigned short            PerKeyLightingFeature  = 0x8081;
    constexpr unsigned short            OnboardProfilesFeature = 0x8100;

    constexpr unsigned char             GetInfoFunction          = 0x00;
    constexpr unsigned char             SetClusterEffectFunction = 0x10;
    constexpr unsigned char             SoftwareControlFunction  = 0x50;
    constexpr unsigned char             SetZonesFunction         = 0x10;
    constexpr unsigned char             FrameEndFunction         = 0x70;
    constexpr unsigned char             SetOnboardModeFunction   = 0x10;
    constexpr unsigned char             ProfilesInfoFunction     = 0x00;
    constexpr unsigned char             SetCurrentProfileFunction = 0x30;
    constexpr unsigned char             GetCurrentProfileFunction = 0x40;
    constexpr unsigned char             MemoryReadFunction       = 0x50;
    constexpr unsigned char             MemoryWriteStartFunction = 0x60;
    constexpr unsigned char             MemoryWriteFunction      = 0x70;
    constexpr unsigned char             MemoryWriteEndFunction   = 0x80;

    constexpr unsigned short            UserDirectorySector   = 0x0000;
    constexpr unsigned short            FirstRomSector        = 0x0100;
    constexpr unsigned short            UnusedSector          = 0xFFFF;
    constexpr std::size_t               DirectoryEntrySize    = 4;
    constexpr std::size_t               MemoryChunkSize       = 16;
    constexpr std::size_t               ProfileLedOffset      = 208;
    constexpr std::size_t               ProfileLedEntrySize   = 11;
    constexpr unsigned int              LargestSectorSize     = 4096;
    constexpr unsigned char             OldestProfileFormat   = 1;
    constexpr unsigned char             NewestProfileFormat   = 5;

    constexpr unsigned char             AllClusters        = 0xFF;
    constexpr unsigned char             PersistToMemory    = 0x01;
    constexpr unsigned char             SetRequest         = 0x01;
    constexpr unsigned char             SoftwareMode       = 0x03;
    constexpr unsigned char             SoftwareFlags      = 0x07;
    constexpr unsigned char             OnboardMode        = 0x01;
    constexpr unsigned char             ZoneListEnd        = 0xFF;
    constexpr unsigned char             FixedColourMarker  = 0x02;
    constexpr std::size_t               ParameterSize      = 10;

    std::string Hex(unsigned int value)
    {
        char text[8];
        std::snprintf(text, sizeof(text), "0x%02x", value);
        return text;
    }
}

ByteVector MouseEffect::Parameters() const
{
    ByteVector          parameters(ParameterSize, 0);
    const unsigned char periodHigh = static_cast<unsigned char>(period >> 8 & 0xFF);
    const unsigned char periodLow  = static_cast<unsigned char>(period & 0xFF);
    const unsigned char red        = static_cast<unsigned char>(RGBGetRValue(colour));
    const unsigned char green      = static_cast<unsigned char>(RGBGetGValue(colour));
    const unsigned char blue       = static_cast<unsigned char>(RGBGetBValue(colour));

    switch(effectId)
    {
        case Static:
            parameters[0] = red;
            parameters[1] = green;
            parameters[2] = blue;
            if(red || green || blue)
            {
                parameters[3] = FixedColourMarker;
            }
            break;
        case Breathing:
            parameters[0] = red;
            parameters[1] = green;
            parameters[2] = blue;
            parameters[3] = periodHigh;
            parameters[4] = periodLow;
            parameters[6] = brightness;
            break;
        case Cycle:
            parameters[5] = periodHigh;
            parameters[6] = periodLow;
            parameters[7] = brightness;
            break;
        case Wave:
            parameters[6] = periodLow;
            parameters[7] = direction;
            parameters[8] = std::max<unsigned char>(1, brightness);
            parameters[9] = periodHigh;
            break;
        default:
            break;
    }

    return parameters;
}

ByteVector MouseEffect::ProfileEntry() const
{
    ByteVector       entry{ static_cast<unsigned char>(effectId & 0xFF) };
    const ByteVector parameters = Parameters();
    entry.insert(entry.end(), parameters.begin(), parameters.end());
    return entry;
}

HidppProtocol::HidppProtocol(HidTransport& transportReference)
    : transport(transportReference)
{
    transport.Drain();
}

ByteVector HidppProtocol::Request(unsigned char featureIndex, unsigned char function, const ByteVector& parameters)
{
    const unsigned char functionByte = static_cast<unsigned char>(function | SoftwareId);
    ByteVector          report{ LongReport, DeviceIndex, featureIndex, functionByte };
    report.insert(report.end(), parameters.begin(), parameters.end());
    report.resize(LongReportSize, 0);
    transport.Write(report);

    const auto deadline = std::chrono::steady_clock::now() + ReplyTimeout;

    while(std::chrono::steady_clock::now() < deadline)
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        const std::optional<ByteVector> reply = transport.Read(std::max(remaining, std::chrono::milliseconds(0)));

        if(!reply.has_value())
        {
            break;
        }

        if(reply->size() < 6 || (*reply)[1] != DeviceIndex)
        {
            continue;
        }

        if((*reply)[2] == ErrorMarker && (*reply)[3] == featureIndex && (*reply)[4] == functionByte)
        {
            throw DeviceError("the mouse rejected feature " + std::to_string(featureIndex) + " function " + Hex(function)
                              + " with HID++ error " + std::to_string((*reply)[5]));
        }

        if((*reply)[2] == featureIndex && (*reply)[3] == functionByte)
        {
            return ByteVector(reply->begin() + 4, reply->end());
        }
    }

    throw DeviceError("the mouse did not answer feature " + std::to_string(featureIndex) + " function " + Hex(function));
}

std::optional<unsigned char> HidppProtocol::FeatureIndex(unsigned short featureId)
{
    const ByteVector reply = Request(RootFeatureIndex, GetFeatureFunction,
                                     { static_cast<unsigned char>(featureId >> 8), static_cast<unsigned char>(featureId & 0xFF) });

    if(reply.empty() || reply[0] == 0)
    {
        return std::nullopt;
    }
    return reply[0];
}

G203Protocol::G203Protocol(HidTransport& transportReference)
    : hidpp(transportReference)
{
    const std::optional<unsigned char> rgbEffectsIndex     = hidpp.FeatureIndex(RgbEffectsFeature);
    const std::optional<unsigned char> perKeyLightingIndex = hidpp.FeatureIndex(PerKeyLightingFeature);

    if(!rgbEffectsIndex.has_value() || !perKeyLightingIndex.has_value())
    {
        throw DeviceError("the mouse lacks the RGB effects (0x8071) or per-LED lighting (0x8081) feature");
    }

    rgbEffects      = *rgbEffectsIndex;
    perKeyLighting  = *perKeyLightingIndex;
    onboardProfiles = hidpp.FeatureIndex(OnboardProfilesFeature);
    ReadClusters();
}

void G203Protocol::StoreEffect(const MouseEffect& effect)
{
    for(std::size_t cluster = 0; cluster < clusters.size(); cluster++)
    {
        const auto effectIndex = clusters[cluster].find(effect.effectId);

        if(effectIndex == clusters[cluster].end())
        {
            throw DeviceError("cluster " + std::to_string(cluster) + " of the mouse has no effect " + Hex(effect.effectId));
        }

        ByteVector parameters{ static_cast<unsigned char>(cluster), effectIndex->second };
        const ByteVector effectParameters = effect.Parameters();
        parameters.insert(parameters.end(), effectParameters.begin(), effectParameters.end());
        parameters.push_back(PersistToMemory);
        hidpp.Request(rgbEffects, SetClusterEffectFunction, parameters);
    }
}

void G203Protocol::ClaimSoftwareControl()
{
    hidpp.Request(rgbEffects, SoftwareControlFunction, { SetRequest, SoftwareMode, SoftwareFlags });
}

void G203Protocol::ReleaseSoftwareControl()
{
    hidpp.Request(rgbEffects, SoftwareControlFunction, { SetRequest, 0, 0 });
}

void G203Protocol::ReloadOnboardProfile()
{
    if(onboardProfiles.has_value())
    {
        const unsigned char profileIndex = hidpp.Request(*onboardProfiles, GetCurrentProfileFunction, {}).at(1);
        hidpp.Request(*onboardProfiles, SetCurrentProfileFunction, { 0x00, profileIndex });
    }
}

void G203Protocol::UseOnboardMemory()
{
    if(onboardProfiles.has_value())
    {
        hidpp.Request(*onboardProfiles, SetOnboardModeFunction, { OnboardMode });
    }
}

void G203Protocol::ShowColours(const std::array<RGBColor, LedCount>& colours)
{
    ByteVector parameters;

    for(std::size_t ledIndex = 0; ledIndex < LedCount; ledIndex++)
    {
        parameters.push_back(static_cast<unsigned char>(ledIndex + 1));
        parameters.push_back(static_cast<unsigned char>(RGBGetRValue(colours[ledIndex])));
        parameters.push_back(static_cast<unsigned char>(RGBGetGValue(colours[ledIndex])));
        parameters.push_back(static_cast<unsigned char>(RGBGetBValue(colours[ledIndex])));
    }
    parameters.push_back(ZoneListEnd);

    hidpp.Request(perKeyLighting, SetZonesFunction, parameters);
    hidpp.Request(perKeyLighting, FrameEndFunction, {});
}

bool G203Protocol::StoreProfileLighting(const MouseEffect& effect)
{
    if(!onboardProfiles.has_value())
    {
        throw DeviceError("the mouse has no onboard profiles, so the lighting only lasts until it loses power");
    }

    const ByteVector    info          = hidpp.Request(*onboardProfiles, ProfilesInfoFunction, {});
    const unsigned char profileFormat = info.at(1);
    const unsigned int  sectorSize    = static_cast<unsigned int>(info.at(7) << 8 | info.at(8));

    if(profileFormat < OldestProfileFormat || profileFormat > NewestProfileFormat
       || sectorSize < ProfileLedOffset + ProfileLedEntrySize + 2 || sectorSize > LargestSectorSize)
    {
        throw DeviceError("the mouse uses an onboard profile layout this plugin doesn't know (format " + std::to_string(profileFormat)
                          + ", " + std::to_string(sectorSize) + "-byte sectors); its profile was left alone");
    }

    const unsigned char profileIndex = hidpp.Request(*onboardProfiles, GetCurrentProfileFunction, {}).at(1);
    const ByteVector    directory    = ReadSector(UserDirectorySector, sectorSize);
    const std::size_t   entryOffset  = (static_cast<std::size_t>(profileIndex) - 1) * DirectoryEntrySize;

    if(!HasValidChecksum(directory) || profileIndex == 0 || entryOffset + DirectoryEntrySize > sectorSize - 2)
    {
        throw DeviceError("the mouse has no valid user profile directory; its profile was left alone");
    }

    const unsigned short sector = static_cast<unsigned short>(directory[entryOffset] << 8 | directory[entryOffset + 1]);

    if(sector == UserDirectorySector || sector == UnusedSector || sector >= FirstRomSector)
    {
        throw DeviceError("the active onboard profile is not a user profile; its profile was left alone");
    }

    const ByteVector profile = ReadSector(sector, sectorSize);

    if(!HasValidChecksum(profile))
    {
        throw DeviceError("the active onboard profile has a bad checksum; it was left alone");
    }

    ByteVector       updated = profile;
    const ByteVector entry   = effect.ProfileEntry();
    std::copy(entry.begin(), entry.end(), updated.begin() + static_cast<std::ptrdiff_t>(ProfileLedOffset));

    if(updated == profile)
    {
        return false;
    }

    const unsigned short checksum = Checksum(updated);
    updated[sectorSize - 2] = static_cast<unsigned char>(checksum >> 8);
    updated[sectorSize - 1] = static_cast<unsigned char>(checksum & 0xFF);
    WriteSector(sector, updated);

    if(ReadSector(sector, sectorSize) != updated)
    {
        throw DeviceError("wrote the onboard profile, but reading it back gave something else");
    }
    return true;
}

unsigned short G203Protocol::Checksum(const ByteVector& sector)
{
    unsigned short checksum = 0xFFFF;

    for(std::size_t index = 0; index + 2 < sector.size(); index++)
    {
        checksum ^= static_cast<unsigned short>(sector[index] << 8);

        for(int bit = 0; bit < 8; bit++)
        {
            checksum = (checksum & 0x8000) ? static_cast<unsigned short>((checksum << 1) ^ 0x1021)
                                           : static_cast<unsigned short>(checksum << 1);
        }
    }
    return checksum;
}

bool G203Protocol::HasValidChecksum(const ByteVector& sector)
{
    const unsigned short stored = static_cast<unsigned short>(sector[sector.size() - 2] << 8 | sector[sector.size() - 1]);
    return Checksum(sector) == stored;
}

ByteVector G203Protocol::ReadSector(unsigned short sector, unsigned int sectorSize)
{
    ByteVector data(sectorSize, 0);

    for(std::size_t offset = 0; offset < sectorSize; offset += MemoryChunkSize)
    {
        const std::size_t readOffset = std::min<std::size_t>(offset, sectorSize - MemoryChunkSize);
        const ByteVector  chunk      = hidpp.Request(*onboardProfiles, MemoryReadFunction,
                                                     { static_cast<unsigned char>(sector >> 8), static_cast<unsigned char>(sector & 0xFF),
                                                       static_cast<unsigned char>(readOffset >> 8), static_cast<unsigned char>(readOffset & 0xFF) });

        if(chunk.size() < MemoryChunkSize)
        {
            throw DeviceError("the mouse sent a short memory read");
        }
        std::copy_n(chunk.begin(), MemoryChunkSize, data.begin() + static_cast<std::ptrdiff_t>(readOffset));
    }

    return data;
}

void G203Protocol::WriteSector(unsigned short sector, const ByteVector& data)
{
    const std::size_t size = data.size();
    hidpp.Request(*onboardProfiles, MemoryWriteStartFunction,
                  { static_cast<unsigned char>(sector >> 8), static_cast<unsigned char>(sector & 0xFF), 0, 0,
                    static_cast<unsigned char>(size >> 8), static_cast<unsigned char>(size & 0xFF) });

    for(std::size_t offset = 0; offset < size; offset += MemoryChunkSize)
    {
        ByteVector chunk(MemoryChunkSize, 0);
        std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(offset), std::min(MemoryChunkSize, size - offset), chunk.begin());
        hidpp.Request(*onboardProfiles, MemoryWriteFunction, chunk);
    }

    hidpp.Request(*onboardProfiles, MemoryWriteEndFunction, {});
}

void G203Protocol::ReadClusters()
{
    const unsigned char clusterCount = hidpp.Request(rgbEffects, GetInfoFunction, { AllClusters, AllClusters, 0 }).at(2);

    for(unsigned char cluster = 0; cluster < clusterCount; cluster++)
    {
        const unsigned char effectCount = hidpp.Request(rgbEffects, GetInfoFunction, { cluster, AllClusters }).at(4);
        std::map<unsigned short, unsigned char> effects;

        for(unsigned char effectIndex = 0; effectIndex < effectCount; effectIndex++)
        {
            const ByteVector reply    = hidpp.Request(rgbEffects, GetInfoFunction, { cluster, effectIndex, 0, 0 });
            const unsigned short id   = static_cast<unsigned short>(reply.at(2) << 8 | reply.at(3));
            effects.emplace(id, effectIndex);
        }

        clusters.push_back(effects);
    }
}
