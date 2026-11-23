#include "BoardDevice.h"

#include <algorithm>
#include <fstream>
#include "LogManager.h"

namespace
{
    constexpr unsigned char SlowestSpeed          = 0xFF;
    constexpr unsigned char FastestSpeed          = 0x00;
    constexpr unsigned char DefaultSpeed          = 0xE0;
    constexpr unsigned int  AddressableMaximumLeds = 100;
    constexpr std::size_t   DirectPaddingLeds     = 8;
    constexpr unsigned char StaticEffect          = 0x01;

    const std::array<const char*, PolychromeLayout::ZoneCount> ZoneNames =
    {
        "RGB LED 1 Header", "RGB LED 2 Header", "Addressable Header 1", "Addressable Header 2",
        "PCH", "IO Cover", "PCB", "Addressable Header 3/Audio",
    };

    const std::array<BoardModeDefinition, 16> Modes =
    {{
        { "Off",            0x00, false, false },
        { "Static",         0x01, false, true  },
        { "Breathing",      0x02, true,  true  },
        { "Strobe",         0x03, true,  true  },
        { "Spectrum Cycle", 0x04, true,  false },
        { "Random",         0x05, true,  false },
        { "Music",          0x06, false, false },
        { "Wave",           0x07, true,  false },
        { "Spring",         0x08, true,  false },
        { "Stack",          0x09, true,  false },
        { "Cram",           0x0A, true,  false },
        { "Scan",           0x0B, true,  false },
        { "Neon",           0x0C, false, false },
        { "Water",          0x0D, true,  false },
        { "Rainbow",        0x0E, true,  false },
        { "Direct",         0x0F, false, true  },
    }};

    std::string ReadBoardName()
    {
        std::ifstream file("/sys/class/dmi/id/board_name");
        std::string   name;
        std::getline(file, name);
        return name.empty() ? "Polychrome board" : name;
    }
}

std::optional<std::string> BoardDevice::FindPath()
{
    const std::vector<HidrawNode> nodes = HidrawNode::Find(VendorId, ProductId);

    if(nodes.empty())
    {
        return std::nullopt;
    }
    return nodes.front().devicePath;
}

BoardDevice::BoardDevice(OpenRGBPluginAPIInterface* pluginApiPointer, PluginLog logFunction, const std::string& devicePath,
                         const nlohmann::json& polychromeSettings, TransportOpener openTransport)
    : VirtualDevice(pluginApiPointer, std::move(logFunction)),
      boardName("ASRock " + ReadBoardName()),
      location("HID: " + devicePath)
{
    handle   = openTransport(devicePath);
    protocol = std::make_unique<PolychromeProtocol>(*handle);
    layout   = protocol->ReadLayout();
    zones    = layout.PresentZones();

    const nlohmann::json swapSettings = polychromeSettings.is_object() && polychromeSettings.contains("RGSwap")
                                      ? polychromeSettings["RGSwap"] : nlohmann::json::object();

    for(std::size_t zone = 0; zone < PolychromeLayout::ZoneCount; zone++)
    {
        const bool settingSwap = swapSettings.contains(ZoneNames[zone]) && swapSettings[ZoneNames[zone]].is_boolean()
                              && swapSettings[ZoneNames[zone]].get<bool>();
        swapped[zone] = (layout.rgbSwapMask >> zone & 1) || settingSwap;
    }

    std::vector<PolychromeZoneState> zoneStates;

    for(unsigned char zone : zones)
    {
        zoneSizes.push_back(std::min<unsigned int>(layout.ledCounts[zone], IsAddressable(zone) ? AddressableMaximumLeds : 255));
        zoneStates.push_back(protocol->ReadZone(zone, swapped[zone]));
    }

    const int          activeMode = zoneStates.empty() ? 0 : ModeIndexForEffect(zoneStates.front().effect);
    const unsigned int speed      = zoneStates.empty() ? DefaultSpeed : zoneStates.front().speed;
    RegisterController(BuildSetup(activeMode, speed));

    unsigned int ledIndex = 0;

    for(std::size_t zoneIndex = 0; zoneIndex < zones.size(); zoneIndex++)
    {
        for(unsigned int zoneLed = 0; zoneLed < zoneSizes[zoneIndex]; zoneLed++)
        {
            Controller()->SetColor(ledIndex++, zoneStates[zoneIndex].color);
        }
    }

    Report(LL_INFO, "found at " + devicePath + " with " + std::to_string(zones.size()) + " zones");
}

BoardDevice::~BoardDevice()
{
    UnregisterController();
}

std::string BoardDevice::DeviceName() const
{
    return boardName;
}

void BoardDevice::ApplyLeds()
{
    std::lock_guard<std::recursive_mutex> lock(ioMutex);
    WriteLive(ReadState(), false);
}

void BoardDevice::ApplyMode()
{
    std::lock_guard<std::recursive_mutex> lock(ioMutex);
    WriteLive(ReadState(), true);
}

void BoardDevice::Save()
{
    std::lock_guard<std::recursive_mutex> lock(ioMutex);
    const ControllerState      state      = ReadState();
    const BoardModeDefinition& mode       = ModeAt(state.mode.index);
    const bool                 isDirect   = mode.effect == PolychromeProtocol::DirectEffect;
    const unsigned char        effect     = isDirect ? StaticEffect : mode.effect;
    const unsigned char        speed      = static_cast<unsigned char>(state.mode.speed);
    const std::vector<RGBColor> colours   = ZoneFirstColours(state);
    const ByteVector           ledCounts  = LedCountsHeader();

    if(!std::equal(ledCounts.begin(), ledCounts.end(), layout.ledCounts.begin()))
    {
        protocol->WriteHeader(PolychromeProtocol::LedCountsHeader, ledCounts);
    }
    protocol->WriteHeader(PolychromeProtocol::RgbSwapHeader, { SwapMask() });

    std::vector<std::pair<unsigned char, PolychromeZoneState>> planned;

    for(std::size_t zoneIndex = 0; zoneIndex < zones.size(); zoneIndex++)
    {
        if(zoneSizes[zoneIndex] == 0)
        {
            continue;
        }

        PolychromeZoneState zoneState;
        zoneState.effect = effect;
        zoneState.color  = colours[zoneIndex];
        zoneState.speed  = speed;
        protocol->SetZone(zones[zoneIndex], zoneState, swapped[zones[zoneIndex]]);
        planned.emplace_back(zones[zoneIndex], zoneState);
    }

    protocol->Commit();
    std::copy(ledCounts.begin(), ledCounts.end(), layout.ledCounts.begin());
    layout.rgbSwapMask = SwapMask();

    std::string mismatches;

    if(protocol->ReadHeader(PolychromeProtocol::RgbSwapHeader)[0] != SwapMask())
    {
        mismatches += " rg-swap";
    }

    for(const auto& [zone, zoneState] : planned)
    {
        if(!(protocol->ReadZone(zone, swapped[zone]) == zoneState))
        {
            mismatches += std::string(" ") + ZoneNames[zone] + ",";
        }
    }

    if(isDirect)
    {
        WriteLive(state, true);
    }

    if(!mismatches.empty())
    {
        throw DeviceError("committed, but the read-back differs for:" + mismatches);
    }

    Report(LL_INFO, isDirect ? "saved to the board; Direct can't be stored, so it was stored as Static with each zone's first colour"
                             : "saved to the board as " + std::string(mode.name));
}

void BoardDevice::ResizeZone(int zoneIndex)
{
    std::lock_guard<std::recursive_mutex> lock(ioMutex);

    if(zoneIndex < 0 || static_cast<std::size_t>(zoneIndex) >= zones.size() || !IsAddressable(zones[zoneIndex]))
    {
        return;
    }

    zoneSizes[zoneIndex] = std::min(Controller()->GetZoneLEDsCount(static_cast<unsigned int>(zoneIndex)), AddressableMaximumLeds);
    protocol->WriteHeader(PolychromeProtocol::LedCountsHeader, LedCountsHeader());

    const ControllerState state = ReadState();
    ReplaceController(BuildSetup(state.mode.index, state.mode.speed));
    Report(LL_INFO, std::string(ZoneNames[zones[zoneIndex]]) + " now has " + std::to_string(zoneSizes[zoneIndex])
                    + " LEDs; press Save to device to keep it after a restart");
}

RGBController_Setup BoardDevice::BuildSetup(int activeMode, unsigned int speed) const
{
    RGBController_Setup setup{};
    setup.name        = boardName;
    setup.vendor      = "ASRock";
    setup.description = BuiltInDescription;
    setup.location    = location;
    setup.active_mode = activeMode;
    setup.type        = DEVICE_TYPE_MOTHERBOARD;

    for(const BoardModeDefinition& definition : Modes)
    {
        mode boardMode;
        boardMode.name       = definition.name;
        boardMode.value      = definition.effect;
        boardMode.flags      = MODE_FLAG_MANUAL_SAVE;
        boardMode.color_mode = definition.hasColours ? MODE_COLORS_PER_LED : MODE_COLORS_NONE;

        if(definition.hasColours)
        {
            boardMode.flags |= MODE_FLAG_HAS_PER_LED_COLOR;
        }

        if(definition.hasSpeed)
        {
            boardMode.flags     |= MODE_FLAG_HAS_SPEED;
            boardMode.speed_min  = SlowestSpeed;
            boardMode.speed_max  = FastestSpeed;
            boardMode.speed      = speed;
        }

        setup.modes.push_back(boardMode);
    }

    for(std::size_t zoneIndex = 0; zoneIndex < zones.size(); zoneIndex++)
    {
        const unsigned char zoneNumber = zones[zoneIndex];
        zone boardZone;
        boardZone.name       = ZoneNames[zoneNumber];
        boardZone.type       = ZONE_TYPE_LINEAR;
        boardZone.leds_count = zoneSizes[zoneIndex];

        if(IsAddressable(zoneNumber))
        {
            boardZone.leds_min = 0;
            boardZone.leds_max = AddressableMaximumLeds;
            boardZone.flags    = ZONE_FLAG_MANUALLY_CONFIGURABLE_SIZE;
        }
        else
        {
            boardZone.leds_min = zoneSizes[zoneIndex];
            boardZone.leds_max = zoneSizes[zoneIndex];
        }

        setup.zones.push_back(boardZone);

        for(unsigned int zoneLed = 0; zoneLed < zoneSizes[zoneIndex]; zoneLed++)
        {
            led boardLed;
            boardLed.name  = std::string(ZoneNames[zoneNumber]) + ", LED " + std::to_string(zoneLed + 1);
            boardLed.value = static_cast<unsigned int>(zoneIndex);
            setup.leds.push_back(boardLed);
        }
    }

    return setup;
}

unsigned char BoardDevice::SwapMask() const
{
    unsigned char mask = 0;

    for(std::size_t zone = 0; zone < PolychromeLayout::ZoneCount; zone++)
    {
        if(swapped[zone])
        {
            mask |= static_cast<unsigned char>(1u << zone);
        }
    }
    return mask;
}

ByteVector BoardDevice::LedCountsHeader() const
{
    ByteVector ledCounts(layout.ledCounts.begin(), layout.ledCounts.end());

    for(std::size_t zoneIndex = 0; zoneIndex < zones.size(); zoneIndex++)
    {
        if(IsAddressable(zones[zoneIndex]))
        {
            ledCounts[zones[zoneIndex]] = static_cast<unsigned char>(zoneSizes[zoneIndex]);
        }
    }
    return ledCounts;
}

std::vector<RGBColor> BoardDevice::ZoneFirstColours(const ControllerState& state) const
{
    std::vector<RGBColor> colours;
    std::size_t           firstLed = 0;

    for(unsigned int zoneSize : zoneSizes)
    {
        colours.push_back(zoneSize > 0 && firstLed < state.ledColors.size() ? state.ledColors[firstLed] : 0);
        firstLed += zoneSize;
    }
    return colours;
}

ByteVector BoardDevice::DirectColours(const ControllerState& state) const
{
    ByteVector  wireColours;
    std::size_t ledIndex      = 0;
    std::size_t maximumLeds   = 0;

    for(std::size_t zoneIndex = 0; zoneIndex < zones.size(); zoneIndex++)
    {
        for(unsigned int zoneLed = 0; zoneLed < zoneSizes[zoneIndex]; zoneLed++)
        {
            const RGBColor   colour     = ledIndex < state.ledColors.size() ? state.ledColors[ledIndex] : 0;
            const ByteVector wireColour = PolychromeProtocol::WireColour(colour, swapped[zones[zoneIndex]]);
            wireColours.insert(wireColours.end(), wireColour.begin(), wireColour.end());
            ledIndex++;
        }
        maximumLeds += IsAddressable(zones[zoneIndex]) ? AddressableMaximumLeds : zoneSizes[zoneIndex];
    }

    wireColours.resize((maximumLeds + DirectPaddingLeds) * 3, 0);
    return wireColours;
}

void BoardDevice::WriteLive(const ControllerState& state, bool includeHeader)
{
    const BoardModeDefinition& mode     = ModeAt(state.mode.index);
    const bool                 isDirect = mode.effect == PolychromeProtocol::DirectEffect;

    if(isDirect && !includeHeader)
    {
        protocol->StreamDirect(DirectColours(state));
        return;
    }

    if(includeHeader)
    {
        protocol->WriteHeader(PolychromeProtocol::RgbSwapHeader, { isDirect ? static_cast<unsigned char>(0) : SwapMask() });
    }

    const std::vector<RGBColor> colours = ZoneFirstColours(state);

    for(std::size_t zoneIndex = 0; zoneIndex < zones.size(); zoneIndex++)
    {
        if(zoneSizes[zoneIndex] == 0)
        {
            continue;
        }

        PolychromeZoneState zoneState;
        zoneState.effect = mode.effect;
        zoneState.color  = colours[zoneIndex];
        zoneState.speed  = static_cast<unsigned char>(state.mode.speed);
        protocol->SetZone(zones[zoneIndex], zoneState, swapped[zones[zoneIndex]]);
    }

    if(isDirect)
    {
        protocol->StreamDirect(DirectColours(state));
    }
}

bool BoardDevice::IsAddressable(unsigned char zone)
{
    return zone == 2 || zone == 3 || zone == 7;
}

const BoardModeDefinition& BoardDevice::ModeAt(int modeIndex)
{
    return Modes[static_cast<std::size_t>(std::clamp(modeIndex, 0, static_cast<int>(Modes.size()) - 1))];
}

int BoardDevice::ModeIndexForEffect(unsigned char effect)
{
    for(std::size_t modeIndex = 0; modeIndex < Modes.size(); modeIndex++)
    {
        if(Modes[modeIndex].effect == effect)
        {
            return static_cast<int>(modeIndex);
        }
    }
    return static_cast<int>(Modes.size()) - 1;
}
