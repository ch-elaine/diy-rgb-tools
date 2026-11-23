#include "G203Device.h"

#include <algorithm>
#include <array>
#include "LogManager.h"

namespace
{
    constexpr unsigned int  SlowestSpeed      = 1;
    constexpr unsigned int  FastestSpeed      = 100;
    constexpr unsigned int  DefaultSpeed      = 50;
    constexpr unsigned int  LongestPeriod     = 20000;
    constexpr unsigned int  ShortestPeriod    = 1000;
    constexpr unsigned int  DimmestBrightness = 1;
    constexpr unsigned int  BrightestBrightness = 100;
    constexpr unsigned char WaveLeftToRight   = 1;
    constexpr unsigned char WaveRightToLeft   = 6;
    constexpr RGBColor      DefaultColour     = ToRGBColor(255, 255, 255);

    const ByteVector HidppLongReportDescriptorMark{ 0x85, 0x11 };

    const std::array<MouseModeDefinition, 6> Modes =
    {{
        { "Direct",         MouseEffect::Static,    true,  MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_MANUAL_SAVE,       MODE_COLORS_PER_LED       },
        { "Off",            MouseEffect::Off,       true,  MODE_FLAG_MANUAL_SAVE,                                     MODE_COLORS_NONE          },
        { "Static",         MouseEffect::Static,    true,  MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_MANUAL_SAVE, MODE_COLORS_MODE_SPECIFIC },
        { "Breathing",      MouseEffect::Breathing, false, MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS
                                                           | MODE_FLAG_MANUAL_SAVE,                                   MODE_COLORS_MODE_SPECIFIC },
        { "Spectrum Cycle", MouseEffect::Cycle,     false, MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_MANUAL_SAVE,
                                                                                                                      MODE_COLORS_NONE          },
        { "Color Wave",     MouseEffect::Wave,      false, MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_DIRECTION_LR
                                                           | MODE_FLAG_MANUAL_SAVE,                                   MODE_COLORS_NONE          },
    }};

    const std::array<const char*, G203Protocol::LedCount> LedNames = { "Left", "Center", "Right" };

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

std::optional<std::string> G203Device::FindPath()
{
    for(const HidrawNode& node : HidrawNode::Find(VendorId, ProductId))
    {
        if(node.DescriptorContains(HidppLongReportDescriptorMark))
        {
            return node.devicePath;
        }
    }
    return std::nullopt;
}

G203Device::G203Device(OpenRGBPluginAPIInterface* pluginApiPointer, PluginLog logFunction, const std::string& devicePath,
                       const nlohmann::json& savedSettings, std::function<void(const nlohmann::json&)> savedSettingsChangedHandler,
                       TransportOpener openTransport)
    : VirtualDevice(pluginApiPointer, std::move(logFunction)),
      savedSettingsChanged(std::move(savedSettingsChangedHandler)),
      location("HID: " + devicePath)
{
    handle   = openTransport(devicePath);
    protocol = std::make_unique<G203Protocol>(*handle);
    RegisterController(BuildSetup(savedSettings));

    const RGBColor colour = SettingOr<RGBColor>(savedSettings, "colour", DefaultColour);

    for(unsigned int ledIndex = 0; ledIndex < G203Protocol::LedCount; ledIndex++)
    {
        Controller()->SetColor(ledIndex, colour);
    }

    Report(LL_INFO, "found at " + devicePath + "; it keeps its stored effect until you change something");
}

G203Device::~G203Device()
{
    UnregisterController();

    try
    {
        std::lock_guard<std::mutex> lock(ioMutex);
        ReleaseIfClaimed();
        protocol->UseOnboardMemory();
    }
    catch(const std::exception&)
    {
    }
}

std::string G203Device::DeviceName() const
{
    return "Logitech G203 LIGHTSYNC";
}

void G203Device::ApplyLeds()
{
    std::lock_guard<std::mutex> lock(ioMutex);
    Preview(ReadState());
}

void G203Device::ApplyMode()
{
    std::lock_guard<std::mutex> lock(ioMutex);
    Preview(ReadState());
}

void G203Device::Save()
{
    std::lock_guard<std::mutex> lock(ioMutex);
    const ControllerState      state  = ReadState();
    const MouseModeDefinition& mode   = ModeAt(state.mode.index);
    const MouseEffect          effect = EffectFor(state);

    protocol->StoreEffect(effect);
    protocol->StoreProfileLighting(effect);
    savedSettingsChanged(SettingsFor(state, effect));

    if(std::string(mode.name) != "Direct")
    {
        ReleaseIfClaimed();
        protocol->ReloadOnboardProfile();
    }

    const bool isDark = effect.effectId == MouseEffect::Off || (effect.effectId == MouseEffect::Static && effect.colour == 0);
    std::string message = std::string(mode.name) == "Direct"
                        ? "saved; Direct can't be stored, so the mouse stores Static with the left LED's colour"
                        : "saved " + std::string(mode.name) + " to the mouse's onboard profile, so it comes back after a restart";
    if(isDark)
    {
        message += " (it stays dark after a restart)";
    }
    Report(LL_INFO, message);
}

RGBController_Setup G203Device::BuildSetup(const nlohmann::json& savedSettings) const
{
    const std::string  savedMode   = SettingOr<std::string>(savedSettings, "mode", "Static");
    const RGBColor     savedColour = SettingOr<RGBColor>(savedSettings, "colour", DefaultColour);

    RGBController_Setup setup{};
    setup.name        = DeviceName();
    setup.vendor      = "Logitech";
    setup.description = "Logitech G203 LIGHTSYNC (DIY RGB Tools plugin)";
    setup.location    = location;
    setup.type        = DEVICE_TYPE_MOUSE;
    setup.active_mode = 2;

    for(std::size_t modeIndex = 0; modeIndex < Modes.size(); modeIndex++)
    {
        const MouseModeDefinition& definition = Modes[modeIndex];
        mode mouseMode;
        mouseMode.name       = definition.name;
        mouseMode.value      = static_cast<int>(modeIndex);
        mouseMode.flags      = definition.flags;
        mouseMode.color_mode = definition.colourMode;

        if(definition.flags & MODE_FLAG_HAS_MODE_SPECIFIC_COLOR)
        {
            mouseMode.colors_min = 1;
            mouseMode.colors_max = 1;
            mouseMode.colors     = { savedColour };
        }

        if(definition.flags & MODE_FLAG_HAS_SPEED)
        {
            mouseMode.speed_min = SlowestSpeed;
            mouseMode.speed_max = FastestSpeed;
            mouseMode.speed     = SettingOr<unsigned int>(savedSettings, "speed", DefaultSpeed);
        }

        if(definition.flags & MODE_FLAG_HAS_BRIGHTNESS)
        {
            mouseMode.brightness_min = DimmestBrightness;
            mouseMode.brightness_max = BrightestBrightness;
            mouseMode.brightness     = SettingOr<unsigned int>(savedSettings, "brightness", BrightestBrightness);
        }

        if(definition.flags & MODE_FLAG_HAS_DIRECTION_LR)
        {
            mouseMode.direction = SettingOr<unsigned int>(savedSettings, "direction", MODE_DIRECTION_RIGHT);
        }

        if(savedMode == definition.name)
        {
            setup.active_mode = static_cast<int>(modeIndex);
        }

        setup.modes.push_back(mouseMode);
    }

    zone mouseZone;
    mouseZone.name       = "Mouse";
    mouseZone.type       = ZONE_TYPE_LINEAR;
    mouseZone.leds_count = G203Protocol::LedCount;
    mouseZone.leds_min   = G203Protocol::LedCount;
    mouseZone.leds_max   = G203Protocol::LedCount;
    setup.zones.push_back(mouseZone);

    for(std::size_t ledIndex = 0; ledIndex < G203Protocol::LedCount; ledIndex++)
    {
        led mouseLed;
        mouseLed.name  = LedNames[ledIndex];
        mouseLed.value = static_cast<unsigned int>(ledIndex + 1);
        setup.leds.push_back(mouseLed);
    }

    return setup;
}

void G203Device::Preview(const ControllerState& state)
{
    const MouseModeDefinition& mode = ModeAt(state.mode.index);

    if(!mode.previewsLive)
    {
        ReleaseIfClaimed();
        const MouseEffect effect = EffectFor(state);
        protocol->StoreEffect(effect);
        savedSettingsChanged(SettingsFor(state, effect));
        return;
    }

    std::array<RGBColor, G203Protocol::LedCount> colours{};

    for(std::size_t ledIndex = 0; ledIndex < G203Protocol::LedCount; ledIndex++)
    {
        if(std::string(mode.name) == "Direct")
        {
            colours[ledIndex] = ledIndex < state.ledColors.size() ? state.ledColors[ledIndex] : 0;
        }
        else if(mode.effectId == MouseEffect::Static)
        {
            colours[ledIndex] = state.mode.FirstColor(0);
        }
    }

    ClaimIfNeeded();
    protocol->ShowColours(colours);
}

MouseEffect G203Device::EffectFor(const ControllerState& state) const
{
    const MouseModeDefinition& mode = ModeAt(state.mode.index);
    MouseEffect                effect;
    effect.effectId = mode.effectId;

    if(std::string(mode.name) == "Direct")
    {
        effect.colour = state.ledColors.empty() ? 0 : state.ledColors.front();
    }
    else if(mode.flags & MODE_FLAG_HAS_MODE_SPECIFIC_COLOR)
    {
        effect.colour = state.mode.FirstColor(0);
    }

    if(mode.flags & MODE_FLAG_HAS_SPEED)
    {
        effect.period = PeriodForSpeed(state.mode.speed);
    }

    if(mode.flags & MODE_FLAG_HAS_BRIGHTNESS)
    {
        effect.brightness = static_cast<unsigned char>(std::clamp(state.mode.brightness, DimmestBrightness, BrightestBrightness));
    }

    effect.direction = state.mode.direction == MODE_DIRECTION_LEFT ? WaveRightToLeft : WaveLeftToRight;
    return effect;
}

nlohmann::json G203Device::SettingsFor(const ControllerState& state, const MouseEffect& storedEffect) const
{
    const MouseModeDefinition& mode = ModeAt(state.mode.index);
    nlohmann::json             settings;
    settings["mode"]       = std::string(mode.name) == "Direct" ? "Static" : mode.name;
    settings["colour"]     = storedEffect.colour;
    settings["speed"]      = state.mode.speed;
    settings["brightness"] = state.mode.brightness;
    settings["direction"]  = state.mode.direction;
    return settings;
}

void G203Device::ClaimIfNeeded()
{
    if(!softwareControlClaimed)
    {
        protocol->ClaimSoftwareControl();
        softwareControlClaimed = true;
    }
}

void G203Device::ReleaseIfClaimed()
{
    if(softwareControlClaimed)
    {
        protocol->ReleaseSoftwareControl();
        softwareControlClaimed = false;
    }
}

const MouseModeDefinition& G203Device::ModeAt(int modeIndex)
{
    return Modes[static_cast<std::size_t>(std::clamp(modeIndex, 0, static_cast<int>(Modes.size()) - 1))];
}

unsigned int G203Device::PeriodForSpeed(unsigned int speed)
{
    const unsigned int clampedSpeed = std::clamp(speed, SlowestSpeed, FastestSpeed);
    return LongestPeriod - (clampedSpeed - SlowestSpeed) * (LongestPeriod - ShortestPeriod) / (FastestSpeed - SlowestSpeed);
}
