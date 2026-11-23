#include "Ak820Device.h"

#include <algorithm>
#include <array>
#include "Hidraw.h"
#include "LogManager.h"

namespace
{
    constexpr unsigned int  MaximumLevel  = 5;
    constexpr unsigned int  DefaultSpeed  = 3;
    constexpr unsigned char KeyboardLeft  = 0;
    constexpr unsigned char KeyboardDown  = 1;
    constexpr unsigned char KeyboardUp    = 2;
    constexpr unsigned char KeyboardRight = 3;
    constexpr RGBColor      DefaultColour = ToRGBColor(255, 255, 255);

    const std::array<KeyboardModeDefinition, 20> Modes =
    {{
        { "Off",        0x00, false, false, 0                          },
        { "Static",     0x01, true,  false, 0                          },
        { "Single On",  0x02, true,  true,  0                          },
        { "Single Off", 0x03, true,  true,  0                          },
        { "Glittering", 0x04, true,  true,  0                          },
        { "Falling",    0x05, true,  true,  0                          },
        { "Colourful",  0x06, true,  true,  0                          },
        { "Breath",     0x07, true,  true,  0                          },
        { "Spectrum",   0x08, true,  true,  0                          },
        { "Outward",    0x09, true,  true,  0                          },
        { "Scrolling",  0x0A, true,  true,  MODE_FLAG_HAS_DIRECTION_UD },
        { "Rolling",    0x0B, true,  true,  MODE_FLAG_HAS_DIRECTION_LR },
        { "Rotating",   0x0C, true,  true,  0                          },
        { "Explode",    0x0D, true,  true,  0                          },
        { "Launch",     0x0E, true,  true,  0                          },
        { "Ripples",    0x0F, true,  true,  0                          },
        { "Flowing",    0x10, true,  true,  MODE_FLAG_HAS_DIRECTION_LR },
        { "Pulsating",  0x11, true,  true,  0                          },
        { "Tilt",       0x12, true,  true,  MODE_FLAG_HAS_DIRECTION_LR },
        { "Shuttle",    0x13, true,  true,  0                          },
    }};

    template<typename Value>
    Value SettingOr(const nlohmann::json& settings, const char* key, Value fallback)
    {
        if(settings.is_object() && settings.contains(key))
        {
            try
            {
                return settings[key].get<Value>();
            }
            catch(const nlohmann::json::exception&)
            {
            }
        }
        return fallback;
    }
}

Ak820Device::Ak820Device(OpenRGBPluginAPIInterface* pluginApiPointer, PluginLog logFunction, const Ak820Interfaces& foundInterfaces,
                         const nlohmann::json& savedSettings, std::function<void(const nlohmann::json&)> savedSettingsChangedHandler,
                         TransportOpener openTransportFunction)
    : VirtualDevice(pluginApiPointer, std::move(logFunction)),
      interfaces(foundInterfaces),
      savedSettingsChanged(std::move(savedSettingsChangedHandler)),
      openTransport(std::move(openTransportFunction))
{
    RegisterController(BuildSetup(savedSettings));
    Report(LL_INFO, "found at " + interfaces.controlPath + " and " + interfaces.displayPath
                    + "; lighting changes are written when you press Save to device");
}

Ak820Device::~Ak820Device()
{
    UnregisterController();

    if(taskThread.joinable())
    {
        taskThread.join();
    }
}

std::string Ak820Device::DeviceName() const
{
    return "Ajazz AK820 Pro";
}

void Ak820Device::StartScreenUpload(const KeyboardAnimation& animation, unsigned int slot)
{
    const ByteVector payload    = animation.Encode();
    const std::size_t chunkCount = payload.size() / Ak820Protocol::ScreenChunkSize;

    StartTask("uploading to screen slot " + std::to_string(slot), [this, payload, slot, chunkCount]
    {
        const std::unique_ptr<HidTransport> control = openTransport(interfaces.controlPath);
        control->LockExclusive();
        const std::unique_ptr<HidTransport> display = openTransport(interfaces.displayPath);
        SetProgress(true, 0, chunkCount, "uploading; keep the keyboard plugged in");
        Ak820Protocol(*control).UploadScreen(*display, payload, slot, [this](std::size_t sent, std::size_t total)
        {
            SetProgress(true, sent, total, "uploading; keep the keyboard plugged in");
        });
        return "screen slot " + std::to_string(slot) + " replaced";
    });
}

void Ak820Device::StartClockSync()
{
    StartTask("setting the clock", [this]
    {
        const std::unique_ptr<HidTransport> control = openTransport(interfaces.controlPath);
        control->LockExclusive();
        const std::tm moment = Ak820Protocol(*control).SyncClock();
        char          text[32];
        std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &moment);
        return std::string("clock set to ") + text;
    });
}

KeyboardTaskProgress Ak820Device::TaskProgress() const
{
    std::lock_guard<std::mutex> lock(progressMutex);
    return progress;
}

void Ak820Device::ApplyLeds()
{
}

void Ak820Device::ApplyMode()
{
}

void Ak820Device::Save()
{
    std::unique_lock<std::mutex> lock = ClaimKeyboard("saving the lighting");
    const ControllerState  state    = ReadState();
    const KeyboardLighting lighting = LightingFor(state);

    const std::unique_ptr<HidTransport> control = openTransport(interfaces.controlPath);
    control->LockExclusive();
    Ak820Protocol(*control).SetLighting(lighting);

    nlohmann::json settings;
    settings["mode"]       = ModeAt(state.mode.index).name;
    settings["colour"]     = state.mode.FirstColor(DefaultColour);
    settings["rainbow"]    = lighting.rainbow;
    settings["brightness"] = state.mode.brightness;
    settings["speed"]      = state.mode.speed;
    settings["direction"]  = state.mode.direction;
    savedSettingsChanged(settings);
    Report(LL_INFO, std::string("saved ") + ModeAt(state.mode.index).name + " to the keyboard");
}

RGBController_Setup Ak820Device::BuildSetup(const nlohmann::json& savedSettings) const
{
    const std::string savedMode   = SettingOr<std::string>(savedSettings, "mode", "Static");
    const RGBColor    savedColour = SettingOr<RGBColor>(savedSettings, "colour", DefaultColour);
    const bool        rainbow     = SettingOr<bool>(savedSettings, "rainbow", false);

    RGBController_Setup setup{};
    setup.name        = DeviceName();
    setup.vendor      = "Ajazz";
    setup.description = "AK820 Pro keyboard (DIY RGB Tools plugin); press Save to device to write the lighting";
    setup.location    = "HID: " + interfaces.controlPath;
    setup.type        = DEVICE_TYPE_KEYBOARD;
    setup.active_mode = 1;

    for(std::size_t modeIndex = 0; modeIndex < Modes.size(); modeIndex++)
    {
        const KeyboardModeDefinition& definition = Modes[modeIndex];
        mode keyboardMode;
        keyboardMode.name       = definition.name;
        keyboardMode.value      = definition.code;
        keyboardMode.flags      = MODE_FLAG_MANUAL_SAVE | definition.directionFlag;
        keyboardMode.color_mode = MODE_COLORS_NONE;

        if(definition.code != 0x00)
        {
            keyboardMode.flags         |= MODE_FLAG_HAS_BRIGHTNESS;
            keyboardMode.brightness_min = 0;
            keyboardMode.brightness_max = MaximumLevel;
            keyboardMode.brightness     = SettingOr<unsigned int>(savedSettings, "brightness", MaximumLevel);
        }

        if(definition.hasColour)
        {
            keyboardMode.flags     |= MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR;
            keyboardMode.colors_min = 1;
            keyboardMode.colors_max = 1;
            keyboardMode.colors     = { savedColour };
            keyboardMode.color_mode = rainbow ? MODE_COLORS_RANDOM : MODE_COLORS_MODE_SPECIFIC;
        }

        if(definition.hasSpeed)
        {
            keyboardMode.flags    |= MODE_FLAG_HAS_SPEED;
            keyboardMode.speed_min = 0;
            keyboardMode.speed_max = MaximumLevel;
            keyboardMode.speed     = SettingOr<unsigned int>(savedSettings, "speed", DefaultSpeed);
        }

        if(definition.directionFlag == MODE_FLAG_HAS_DIRECTION_UD)
        {
            keyboardMode.direction = SettingOr<unsigned int>(savedSettings, "direction", MODE_DIRECTION_UP);
        }
        else if(definition.directionFlag == MODE_FLAG_HAS_DIRECTION_LR)
        {
            keyboardMode.direction = SettingOr<unsigned int>(savedSettings, "direction", MODE_DIRECTION_LEFT);
        }

        if(savedMode == definition.name)
        {
            setup.active_mode = static_cast<int>(modeIndex);
        }

        setup.modes.push_back(keyboardMode);
    }

    zone backlight;
    backlight.name       = "Backlight";
    backlight.type       = ZONE_TYPE_SINGLE;
    backlight.leds_count = 1;
    backlight.leds_min   = 1;
    backlight.leds_max   = 1;
    setup.zones.push_back(backlight);

    led backlightLed;
    backlightLed.name = "Backlight";
    setup.leds.push_back(backlightLed);
    return setup;
}

KeyboardLighting Ak820Device::LightingFor(const ControllerState& state) const
{
    const KeyboardModeDefinition& mode = ModeAt(state.mode.index);
    KeyboardLighting              lighting;
    const RGBColor                colour = mode.hasColour ? state.mode.FirstColor(DefaultColour) : 0;

    lighting.modeCode   = mode.code;
    lighting.red        = static_cast<unsigned char>(RGBGetRValue(colour));
    lighting.green      = static_cast<unsigned char>(RGBGetGValue(colour));
    lighting.blue       = static_cast<unsigned char>(RGBGetBValue(colour));
    lighting.rainbow    = mode.hasColour && state.mode.colorMode == MODE_COLORS_RANDOM;
    lighting.brightness = static_cast<unsigned char>(mode.code == 0x00 ? MaximumLevel : std::min(state.mode.brightness, MaximumLevel));
    lighting.speed      = static_cast<unsigned char>(mode.hasSpeed ? std::min(state.mode.speed, MaximumLevel) : DefaultSpeed);

    switch(mode.directionFlag)
    {
        case MODE_FLAG_HAS_DIRECTION_UD:
            lighting.direction = state.mode.direction == MODE_DIRECTION_DOWN ? KeyboardDown : KeyboardUp;
            break;
        case MODE_FLAG_HAS_DIRECTION_LR:
            lighting.direction = state.mode.direction == MODE_DIRECTION_RIGHT ? KeyboardRight : KeyboardLeft;
            break;
        default:
            lighting.direction = KeyboardLeft;
            break;
    }

    return lighting;
}

std::unique_lock<std::mutex> Ak820Device::ClaimKeyboard(const std::string& action)
{
    std::unique_lock<std::mutex> lock(ioMutex, std::try_to_lock);

    if(!lock.owns_lock())
    {
        throw DeviceError("the keyboard is busy with another task; try " + action + " again when it finishes");
    }
    return lock;
}

void Ak820Device::StartTask(const std::string& description, std::function<std::string()> work)
{
    if(TaskProgress().running)
    {
        throw DeviceError("the keyboard is still busy; wait for the current task to finish");
    }

    if(taskThread.joinable())
    {
        taskThread.join();
    }

    SetProgress(true, 0, 0, description);
    taskThread = std::thread([this, description, work]
    {
        try
        {
            std::unique_lock<std::mutex> lock = ClaimKeyboard(description);
            const std::string            result = work();
            const KeyboardTaskProgress   finished = TaskProgress();
            SetProgress(false, finished.total, finished.total, result);
            Report(LL_INFO, result);
        }
        catch(const std::exception& error)
        {
            const KeyboardTaskProgress failed = TaskProgress();
            SetProgress(false, failed.done, failed.total, description + " failed: " + error.what());
            Report(LL_ERROR, description + " failed: " + error.what());
        }
    });
}

void Ak820Device::SetProgress(bool running, std::size_t done, std::size_t total, const std::string& message)
{
    std::lock_guard<std::mutex> lock(progressMutex);
    progress.running = running;
    progress.done    = done;
    progress.total   = total;
    progress.message = message;
}

const KeyboardModeDefinition& Ak820Device::ModeAt(int modeIndex)
{
    return Modes[static_cast<std::size_t>(std::clamp(modeIndex, 0, static_cast<int>(Modes.size()) - 1))];
}
