#include "CoolerDevice.h"

#include "Ax120SegmentLayout.h"
#include "CoolerServiceFiles.h"
#include "LogManager.h"

namespace
{
    constexpr unsigned int MinimumBrightness = 0;
    constexpr unsigned int MaximumBrightness = 100;
}

CoolerDevice::CoolerDevice(OpenRGBPluginAPIInterface* pluginApiPointer, PluginLog logFunction, const std::string& devicePath,
                           const Ax120PluginSettings& settings, std::function<void()> lightingChangedHandler)
    : VirtualDevice(pluginApiPointer, logFunction),
      lightingChanged(std::move(lightingChangedHandler)),
      lighting(settings.lighting),
      displayOptions(settings.display)
{
    driver = std::make_unique<Ax120DisplayDriver>(settings.display, settings.lighting, logFunction);
    RegisterController(BuildSetup(devicePath));

    for(std::size_t ledIndex = 0; ledIndex < Ax120LedCount; ledIndex++)
    {
        Controller()->SetColor(static_cast<unsigned int>(ledIndex), lighting.colors[ledIndex]);
    }

    driver->Start();
}

CoolerDevice::~CoolerDevice()
{
    driver->Stop();
    UnregisterController();
}

std::string CoolerDevice::DeviceName() const
{
    return "Thermalright AX120 Digital";
}

Ax120Lighting CoolerDevice::Lighting() const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return lighting;
}

void CoolerDevice::SetDisplayOptions(const Ax120DisplayOptions& newDisplayOptions)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        displayOptions = newDisplayOptions;
    }
    driver->SetDisplayOptions(newDisplayOptions);
}

Ax120DriverStatus CoolerDevice::DriverStatus() const
{
    return driver->Status();
}

void CoolerDevice::ApplyLeds()
{
    ReadLightingFromController();
}

void CoolerDevice::ApplyMode()
{
    ReadLightingFromController();
}

void CoolerDevice::Save()
{
    if(!CoolerServiceFiles::IsInstalled())
    {
        throw DeviceError("the cooler service is not installed; press Install cooler service in the plugin's Cooler tab");
    }

    if(!CoolerServiceFiles::SettingsAreWritable())
    {
        throw DeviceError(std::string(CoolerServiceFiles::SettingsPath) + " is not writable by you; press Update cooler service in the Cooler tab");
    }

    Ax120PluginSettings settings;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        settings.lighting = lighting;
        settings.display  = displayOptions;
    }

    CoolerServiceFiles::WriteSettings(CoolerServiceFiles::SettingsPath, settings);
    Report(LL_INFO, "saved; the cooler service shows this whenever OpenRGB is closed");
}

RGBController_Setup CoolerDevice::BuildSetup(const std::string& devicePath) const
{
    RGBController_Setup setup{};
    setup.name        = DeviceName();
    setup.vendor      = "Thermalright";
    setup.description = "AX120 Digital segment display (DIY RGB Tools plugin)";
    setup.location    = "HID: " + devicePath;
    setup.active_mode = 0;
    setup.type        = DEVICE_TYPE_COOLER;

    mode directMode;
    directMode.name           = "Direct";
    directMode.value          = 0;
    directMode.flags          = MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_MANUAL_SAVE;
    directMode.brightness_min = MinimumBrightness;
    directMode.brightness_max = MaximumBrightness;
    directMode.brightness     = lighting.brightness;
    directMode.color_mode     = MODE_COLORS_PER_LED;
    setup.modes.push_back(directMode);

    for(const Ax120ZoneLayout& zoneLayout : Ax120SegmentLayout::Zones())
    {
        zone displayZone;
        displayZone.name       = zoneLayout.name;
        displayZone.type       = zoneLayout.type;
        displayZone.leds_count = zoneLayout.ledCount;
        displayZone.leds_min   = zoneLayout.ledCount;
        displayZone.leds_max   = zoneLayout.ledCount;

        if(!zoneLayout.matrixMap.empty())
        {
            std::vector<unsigned int> matrixMap = zoneLayout.matrixMap;
            displayZone.matrix_map.Set(zoneLayout.matrixHeight, zoneLayout.matrixWidth, matrixMap.data());
        }

        setup.zones.push_back(displayZone);
    }

    const std::vector<std::string>& ledNames = Ax120SegmentLayout::LedNames();

    for(std::size_t ledIndex = 0; ledIndex < ledNames.size(); ledIndex++)
    {
        led displayLed;
        displayLed.name  = ledNames[ledIndex];
        displayLed.value = static_cast<unsigned int>(ledIndex);
        setup.leds.push_back(displayLed);
    }

    return setup;
}

void CoolerDevice::ReadLightingFromController()
{
    const ControllerState state = ReadState();
    Ax120Lighting         newLighting;

    for(std::size_t ledIndex = 0; ledIndex < Ax120LedCount && ledIndex < state.ledColors.size(); ledIndex++)
    {
        newLighting.colors[ledIndex] = state.ledColors[ledIndex];
    }
    newLighting.brightness = state.mode.index >= 0 ? state.mode.brightness : MaximumBrightness;

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        lighting = newLighting;
    }

    driver->SetLighting(newLighting);
    lightingChanged();
}
