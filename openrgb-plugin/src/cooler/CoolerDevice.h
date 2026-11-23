#pragma once

#include <memory>
#include <mutex>
#include <string>
#include "Ax120DisplayDriver.h"
#include "Ax120Settings.h"
#include "VirtualDevice.h"

class CoolerDevice : public VirtualDevice
{
public:
    CoolerDevice(OpenRGBPluginAPIInterface* pluginApiPointer, PluginLog logFunction, const std::string& devicePath,
                 const Ax120PluginSettings& settings, std::function<void()> lightingChangedHandler);
    ~CoolerDevice() override;

    std::string         DeviceName() const override;
    Ax120Lighting       Lighting() const;
    void                SetDisplayOptions(const Ax120DisplayOptions& newDisplayOptions);
    Ax120DriverStatus   DriverStatus() const;

protected:
    void                ApplyLeds() override;
    void                ApplyMode() override;
    void                Save() override;

private:
    std::unique_ptr<Ax120DisplayDriver> driver;
    std::function<void()>               lightingChanged;
    mutable std::mutex                  stateMutex;
    Ax120Lighting                       lighting;
    Ax120DisplayOptions                 displayOptions;

    RGBController_Setup                 BuildSetup(const std::string& devicePath) const;
    void                                ReadLightingFromController();
};
