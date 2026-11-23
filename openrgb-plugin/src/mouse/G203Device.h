#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>
#include "G203Protocol.h"
#include "Hidraw.h"
#include "VirtualDevice.h"

class MouseModeDefinition
{
public:
    const char*     name;
    unsigned short  effectId;
    bool            previewsLive;
    unsigned int    flags;
    unsigned int    colourMode;
};

class G203Device : public VirtualDevice
{
public:
    static constexpr unsigned short VendorId  = 0x046D;
    static constexpr unsigned short ProductId = 0xC092;
    static constexpr const char*    MarkerInName        = "G203";
    static constexpr const char*    GenericDescription  = "Logitech HID++ 2.0 Device";
    static constexpr const char*    LegacyDescription   = "Logitech Mouse Device";
    static constexpr const char*    GenericDetector     = "Logitech HID++ 2.0";
    static constexpr const char*    LegacyDetector      = "Logitech G203 Lightsync";

    static std::optional<std::string>   FindPath();

    G203Device(OpenRGBPluginAPIInterface* pluginApiPointer, PluginLog logFunction, const std::string& devicePath,
               const nlohmann::json& savedSettings, std::function<void(const nlohmann::json&)> savedSettingsChangedHandler,
               TransportOpener openTransport = OpenHidraw);
    ~G203Device() override;

    std::string     DeviceName() const override;

protected:
    void            ApplyLeds() override;
    void            ApplyMode() override;
    void            Save() override;

private:
    std::unique_ptr<HidTransport>               handle;
    std::unique_ptr<G203Protocol>               protocol;
    std::function<void(const nlohmann::json&)>  savedSettingsChanged;
    std::string                                 location;
    bool                                        softwareControlClaimed = false;
    std::mutex                                  ioMutex;

    RGBController_Setup         BuildSetup(const nlohmann::json& savedSettings) const;
    void                        Preview(const ControllerState& state);
    MouseEffect                 EffectFor(const ControllerState& state) const;
    nlohmann::json              SettingsFor(const ControllerState& state, const MouseEffect& storedEffect) const;
    void                        ClaimIfNeeded();
    void                        ReleaseIfClaimed();

    static const MouseModeDefinition&   ModeAt(int modeIndex);
    static unsigned int                 PeriodForSpeed(unsigned int speed);
};
