#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include "OpenRGBPluginInterface.h"

using PluginLog = std::function<void(unsigned int logLevel, const std::string& message)>;

class ModeState
{
public:
    int                     index      = -1;
    std::string             name;
    unsigned int            speed      = 0;
    unsigned int            brightness = 0;
    unsigned int            direction  = 0;
    unsigned int            colorMode  = 0;
    std::vector<RGBColor>   colors;

    RGBColor                FirstColor(RGBColor fallback) const;
};

class ControllerState
{
public:
    ModeState                   mode;
    std::vector<RGBColor>       ledColors;
    std::vector<unsigned int>   zoneLedCounts;
};

class VirtualDevice
{
public:
    VirtualDevice(OpenRGBPluginAPIInterface* pluginApiPointer, PluginLog logFunction);
    VirtualDevice(const VirtualDevice&) = delete;
    VirtualDevice& operator=(const VirtualDevice&) = delete;
    virtual ~VirtualDevice();

    virtual std::string         DeviceName() const = 0;
    std::string                 StatusText() const;

protected:
    OpenRGBPluginAPIInterface*  pluginApi;

    void                        RegisterController(RGBController_Setup setup);
    void                        ReplaceController(RGBController_Setup setup);
    void                        UnregisterController();
    RGBControllerInterface*     Controller() const;
    ControllerState             ReadState() const;
    void                        Report(unsigned int logLevel, const std::string& message);
    void                        Guard(const std::string& action, const std::function<void()>& work);

    virtual void                ApplyLeds() = 0;
    virtual void                ApplyMode() = 0;
    virtual void                Save() = 0;
    virtual void                ResizeZone(int zoneIndex);

private:
    PluginLog                   log;
    RGBControllerInterface*     controller = nullptr;
    mutable std::mutex          statusMutex;
    std::string                 status;

    void                        Attach(RGBController_Setup& setup);

    static void                 UpdateLedsCallback(void* devicePointer);
    static void                 UpdateZoneLedsCallback(void* devicePointer, int zoneIndex);
    static void                 UpdateSingleLedCallback(void* devicePointer, int ledIndex);
    static void                 UpdateModeCallback(void* devicePointer);
    static void                 SaveModeCallback(void* devicePointer);
    static void                 ConfigureZoneCallback(void* devicePointer, int zoneIndex);
};
