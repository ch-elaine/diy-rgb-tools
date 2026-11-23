#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>
#include "Hidraw.h"
#include "PolychromeProtocol.h"
#include "VirtualDevice.h"

class BoardModeDefinition
{
public:
    const char*     name;
    unsigned char   effect;
    bool            hasSpeed;
    bool            hasColours;
};

class BoardDevice : public VirtualDevice
{
public:
    static constexpr unsigned short VendorId  = 0x26CE;
    static constexpr unsigned short ProductId = 0x01A2;
    static constexpr const char*    BuiltInDescription = "ASRock Polychrome USB Device";
    static constexpr const char*    BuiltInDetector    = "ASRock Polychrome USB";

    static std::optional<std::string>   FindPath();

    BoardDevice(OpenRGBPluginAPIInterface* pluginApiPointer, PluginLog logFunction, const std::string& devicePath,
                const nlohmann::json& polychromeSettings, TransportOpener openTransport = OpenHidraw);
    ~BoardDevice() override;

    std::string     DeviceName() const override;

protected:
    void            ApplyLeds() override;
    void            ApplyMode() override;
    void            Save() override;
    void            ResizeZone(int zoneIndex) override;

private:
    std::unique_ptr<HidTransport>                               handle;
    std::unique_ptr<PolychromeProtocol>                         protocol;
    PolychromeLayout                                            layout;
    std::vector<unsigned char>                                  zones;
    std::vector<unsigned int>                                   zoneSizes;
    std::array<bool, PolychromeLayout::ZoneCount>               swapped{};
    std::string                                                 boardName;
    std::string                                                 location;
    std::recursive_mutex                                        ioMutex;

    RGBController_Setup         BuildSetup(int activeMode, unsigned int speed) const;
    unsigned char               SwapMask() const;
    ByteVector                  LedCountsHeader() const;
    std::vector<RGBColor>       ZoneFirstColours(const ControllerState& state) const;
    ByteVector                  DirectColours(const ControllerState& state) const;
    void                        WriteLive(const ControllerState& state, bool includeHeader);

    static bool                 IsAddressable(unsigned char zone);
    static const BoardModeDefinition&   ModeAt(int modeIndex);
    static int                  ModeIndexForEffect(unsigned char effect);
};
