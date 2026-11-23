#include "VirtualDevice.h"

#include <exception>
#include "LogManager.h"

RGBColor ModeState::FirstColor(RGBColor fallback) const
{
    return colors.empty() ? fallback : colors.front();
}

VirtualDevice::VirtualDevice(OpenRGBPluginAPIInterface* pluginApiPointer, PluginLog logFunction)
    : pluginApi(pluginApiPointer),
      log(std::move(logFunction))
{
}

VirtualDevice::~VirtualDevice()
{
    UnregisterController();
}

std::string VirtualDevice::StatusText() const
{
    std::lock_guard<std::mutex> lock(statusMutex);
    return status;
}

void VirtualDevice::RegisterController(RGBController_Setup setup)
{
    Attach(setup);
    controller = pluginApi->CreateVirtualRGBController(&setup);
    pluginApi->RegisterVirtualRGBControllerInThread(controller);
}

void VirtualDevice::ReplaceController(RGBController_Setup setup)
{
    Attach(setup);
    pluginApi->UpdateVirtualRGBController(controller, &setup);
}

void VirtualDevice::UnregisterController()
{
    if(controller == nullptr)
    {
        return;
    }

    pluginApi->UnregisterVirtualRGBController(controller);
    pluginApi->DeleteVirtualRGBController(controller);
    controller = nullptr;
}

RGBControllerInterface* VirtualDevice::Controller() const
{
    return controller;
}

ControllerState VirtualDevice::ReadState() const
{
    ControllerState state;
    state.mode.index = controller->GetActiveMode();

    if(state.mode.index >= 0)
    {
        const unsigned int modeIndex = static_cast<unsigned int>(state.mode.index);
        state.mode.name       = controller->GetModeName(modeIndex);
        state.mode.speed      = controller->GetModeSpeed(modeIndex);
        state.mode.brightness = controller->GetModeBrightness(modeIndex);
        state.mode.direction  = controller->GetModeDirection(modeIndex);
        state.mode.colorMode  = controller->GetModeColorMode(modeIndex);

        for(unsigned int colorIndex = 0; colorIndex < controller->GetModeColorsCount(modeIndex); colorIndex++)
        {
            state.mode.colors.push_back(controller->GetModeColor(modeIndex, colorIndex));
        }
    }

    for(unsigned int ledIndex = 0; ledIndex < controller->GetLEDCount(); ledIndex++)
    {
        state.ledColors.push_back(controller->GetColor(ledIndex));
    }

    for(unsigned int zoneIndex = 0; zoneIndex < controller->GetZoneCount(); zoneIndex++)
    {
        state.zoneLedCounts.push_back(controller->GetZoneLEDsCount(zoneIndex));
    }

    return state;
}

void VirtualDevice::Report(unsigned int logLevel, const std::string& message)
{
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        status = message;
    }
    log(logLevel, "[DIY RGB Tools] " + DeviceName() + ": " + message);
}

void VirtualDevice::Guard(const std::string& action, const std::function<void()>& work)
{
    try
    {
        work();
    }
    catch(const std::exception& error)
    {
        Report(LL_ERROR, action + " failed: " + error.what());
    }
}

void VirtualDevice::ResizeZone(int)
{
}

void VirtualDevice::Attach(RGBController_Setup& setup)
{
    setup.object_ptr            = this;
    setup.flags                 = CONTROLLER_FLAG_VIRTUAL;
    setup.DeviceUpdateLEDs      = &VirtualDevice::UpdateLedsCallback;
    setup.DeviceUpdateZoneLEDs  = &VirtualDevice::UpdateZoneLedsCallback;
    setup.DeviceUpdateSingleLED = &VirtualDevice::UpdateSingleLedCallback;
    setup.DeviceUpdateMode      = &VirtualDevice::UpdateModeCallback;
    setup.DeviceSaveMode        = &VirtualDevice::SaveModeCallback;
    setup.DeviceConfigureZone   = &VirtualDevice::ConfigureZoneCallback;
}

void VirtualDevice::UpdateLedsCallback(void* devicePointer)
{
    VirtualDevice* device = static_cast<VirtualDevice*>(devicePointer);
    device->Guard("Updating the colours", [device] { device->ApplyLeds(); });
}

void VirtualDevice::UpdateZoneLedsCallback(void* devicePointer, int)
{
    UpdateLedsCallback(devicePointer);
}

void VirtualDevice::UpdateSingleLedCallback(void* devicePointer, int)
{
    UpdateLedsCallback(devicePointer);
}

void VirtualDevice::UpdateModeCallback(void* devicePointer)
{
    VirtualDevice* device = static_cast<VirtualDevice*>(devicePointer);
    device->Guard("Changing the mode", [device] { device->ApplyMode(); });
}

void VirtualDevice::SaveModeCallback(void* devicePointer)
{
    VirtualDevice* device = static_cast<VirtualDevice*>(devicePointer);
    device->Guard("Saving to the device", [device] { device->Save(); });
}

void VirtualDevice::ConfigureZoneCallback(void* devicePointer, int zoneIndex)
{
    VirtualDevice* device = static_cast<VirtualDevice*>(devicePointer);
    device->Guard("Resizing the zone", [device, zoneIndex] { device->ResizeZone(zoneIndex); });
}
