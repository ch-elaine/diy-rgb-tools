#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "Hidraw.h"
#include "RGBControllerInterface.h"

class MouseEffect
{
public:
    static constexpr unsigned short Off       = 0x0000;
    static constexpr unsigned short Static    = 0x0001;
    static constexpr unsigned short Cycle     = 0x0003;
    static constexpr unsigned short Wave      = 0x0004;
    static constexpr unsigned short Breathing = 0x000A;

    unsigned short  effectId   = Static;
    RGBColor        colour     = 0;
    unsigned int    period     = 0;
    unsigned char   brightness = 100;
    unsigned char   direction  = 1;

    ByteVector      Parameters() const;
    ByteVector      ProfileEntry() const;
};

class HidppProtocol
{
public:
    static constexpr unsigned char  PluginSoftwareId  = 0x0D;

    HidppProtocol(HidTransport& transportReference, unsigned char softwareIdentifier);

    ByteVector                      Request(unsigned char featureIndex, unsigned char function, const ByteVector& parameters);
    std::optional<unsigned char>    FeatureIndex(unsigned short featureId);

private:
    HidTransport&   transport;
    unsigned char   softwareId;
};

class G203Protocol
{
public:
    static constexpr std::size_t    LedCount  = 3;
    static constexpr unsigned short VendorId  = 0x046D;
    static constexpr unsigned short ProductId = 0xC092;

    static std::optional<std::string>   FindPath();

    explicit G203Protocol(HidTransport& transportReference, unsigned char softwareId = HidppProtocol::PluginSoftwareId);

    void    StoreEffect(const MouseEffect& effect);
    void    ClaimSoftwareControl();
    void    ReleaseSoftwareControl();
    bool    IsRunningOnboardProfile();
    void    ShowOnboardProfile();
    void    ShowColours(const std::array<RGBColor, LedCount>& colours);
    bool    StoreProfileLighting(const MouseEffect& effect);

    static unsigned short   Checksum(const ByteVector& sector);

private:
    HidppProtocol                                       hidpp;
    unsigned char                                       rgbEffects   = 0;
    unsigned char                                       perKeyLighting = 0;
    std::optional<unsigned char>                        onboardProfiles;
    std::vector<std::map<unsigned short, unsigned char>> clusters;

    void        ReadClusters();
    ByteVector  ReadSector(unsigned short sector, unsigned int sectorSize);
    void        WriteSector(unsigned short sector, const ByteVector& data);
    static bool HasValidChecksum(const ByteVector& sector);
};
