#pragma once

#include <ctime>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <nlohmann/json.hpp>
#include "Ak820Protocol.h"
#include "KeyboardAnimation.h"
#include "VirtualDevice.h"

class KeyboardModeDefinition
{
public:
    const char*     name;
    unsigned char   code;
    bool            hasColour;
    bool            hasSpeed;
    unsigned int    directionFlag;
};

class KeyboardTaskProgress
{
public:
    bool            running = false;
    std::size_t     done    = 0;
    std::size_t     total   = 0;
    std::string     message;
};

class Ak820Device : public VirtualDevice
{
public:
    Ak820Device(OpenRGBPluginAPIInterface* pluginApiPointer, PluginLog logFunction, const Ak820Interfaces& foundInterfaces,
                const nlohmann::json& savedSettings, std::function<void(const nlohmann::json&)> savedSettingsChangedHandler,
                TransportOpener openTransport = OpenHidraw);
    ~Ak820Device() override;

    std::string             DeviceName() const override;
    void                    StartScreenUpload(const KeyboardAnimation& animation, unsigned int slot);
    void                    StartClockSync();
    KeyboardTaskProgress    TaskProgress() const;

protected:
    void            ApplyLeds() override;
    void            ApplyMode() override;
    void            Save() override;

private:
    Ak820Interfaces                             interfaces;
    std::function<void(const nlohmann::json&)>  savedSettingsChanged;
    TransportOpener                             openTransport;
    std::mutex                                  ioMutex;
    mutable std::mutex                          progressMutex;
    KeyboardTaskProgress                        progress;
    std::thread                                 taskThread;

    RGBController_Setup         BuildSetup(const nlohmann::json& savedSettings) const;
    KeyboardLighting            LightingFor(const ControllerState& state) const;
    std::unique_lock<std::mutex> ClaimKeyboard(const std::string& action);
    void                        StartTask(const std::string& description, std::function<std::string()> work);
    void                        SetProgress(bool running, std::size_t done, std::size_t total, const std::string& message);

    static const KeyboardModeDefinition&    ModeAt(int modeIndex);
};
