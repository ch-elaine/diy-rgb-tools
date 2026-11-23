#include "DiyRgbToolsPlugin.h"

#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>
#include "Ax120HidDevice.h"
#include "Ax120SettingsWidget.h"
#include "CoolerServiceWidget.h"
#include "KeyboardWidget.h"
#include "LogManager.h"
#include "ResourceManagerCallback.h"

namespace
{
    const std::string   SettingsKey          = "DiyRgbTools";
    const std::string   OldCoolerSettingsKey = "Ax120DigitalPlugin";
    const std::string   PluginName           = "DIY RGB Tools";
    const std::string   CoolerKey            = "cooler";
    const std::string   BoardKey             = "board";
    const std::string   MouseKey             = "mouse";
    const std::string   KeyboardKey          = "keyboard";
    constexpr int       SettingsSaveDelayMilliseconds = 3000;

    const std::vector<std::pair<std::string, std::string>> DeviceKeys =
    {
        { BoardKey,    "ASRock Polychrome board"    },
        { MouseKey,    "Logitech G203 LIGHTSYNC"    },
        { KeyboardKey, "Ajazz AK820 Pro"            },
        { CoolerKey,   "Thermalright AX120 Digital" },
    };
}

DiyRgbToolsPlugin::DiyRgbToolsPlugin()
{
    settingsSaveTimer.setSingleShot(true);
    settingsSaveTimer.setInterval(SettingsSaveDelayMilliseconds);
    connect(&settingsSaveTimer, &QTimer::timeout, this, [this] { SaveSettings(); });
    serviceManager = new CoolerServiceManager(this);
}

DiyRgbToolsPlugin::~DiyRgbToolsPlugin()
{
    Unload();
}

OpenRGBPluginInfo DiyRgbToolsPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo pluginInfo;
    pluginInfo.Name            = PluginName;
    pluginInfo.Description     = "Drivers with a real Save to device for the ASRock Polychrome board, the Logitech G203, "
                                 "the Ajazz AK820 Pro (with screen upload) and the Thermalright AX120 Digital";
    pluginInfo.Version         = "2.0";
    pluginInfo.Location        = OPENRGB_PLUGIN_LOCATION_TOP;
    pluginInfo.Label           = PluginName;
    pluginInfo.ProtocolVersion = 0;
    return pluginInfo;
}

unsigned int DiyRgbToolsPlugin::GetPluginAPIVersion()
{
    return OPENRGB_PLUGIN_API_VERSION;
}

void DiyRgbToolsPlugin::Load(OpenRGBPluginAPIInterface* pluginApiPointer)
{
    pluginApi = pluginApiPointer;
    settings  = pluginApi->GetSettings(SettingsKey);

    if(!settings.is_object())
    {
        settings = nlohmann::json::object();
    }

    if(!settings.contains(CoolerKey))
    {
        const nlohmann::json oldCoolerSettings = pluginApi->GetSettings(OldCoolerSettingsKey);
        settings[CoolerKey] = oldCoolerSettings.is_object() ? oldCoolerSettings : nlohmann::json::object();
    }

    coolerSettings    = Ax120PluginSettings::FromJson(settings[CoolerKey]);
    lastSavedSettings = settings;
    CreateMissingDevices();
}

QWidget* DiyRgbToolsPlugin::GetWidget()
{
    QTabWidget* tabs = new QTabWidget();

    tabs->addTab(new DevicesWidget([this] { return Overview(); }, [this] { TurnOffBuiltInDrivers(); }), "Devices");

    QWidget*     coolerTab    = new QWidget();
    QVBoxLayout* coolerLayout = new QVBoxLayout(coolerTab);
    coolerLayout->addWidget(new Ax120SettingsWidget(
        [this] { return coolerSettings.display; },
        [this](const Ax120DisplayOptions& displayOptions) { ApplyDisplayOptions(displayOptions); },
        [this]() -> std::optional<Ax120DriverStatus>
        {
            if(!cooler)
            {
                return std::nullopt;
            }
            return cooler->DriverStatus();
        }));
    coolerLayout->addWidget(new CoolerServiceWidget(serviceManager, [this] { return CurrentCoolerSettings(); }));
    coolerLayout->addStretch();
    tabs->addTab(coolerTab, "Cooler");

    tabs->addTab(new KeyboardWidget([this] { return keyboard.get(); }), "Keyboard");
    return tabs;
}

QMenu* DiyRgbToolsPlugin::GetTrayMenu()
{
    return nullptr;
}

void DiyRgbToolsPlugin::Unload()
{
    if(pluginApi == nullptr)
    {
        return;
    }

    settingsSaveTimer.stop();

    if(cooler)
    {
        coolerSettings.lighting = cooler->Lighting();
        settings[CoolerKey]     = coolerSettings.ToJson();
    }

    keyboard.reset();
    mouse.reset();
    board.reset();
    cooler.reset();
    SaveSettings();
    pluginApi = nullptr;
}

void DiyRgbToolsPlugin::OnProfileAboutToLoad()
{
}

void DiyRgbToolsPlugin::OnProfileLoad(nlohmann::json profileData)
{
    if(!profileData.is_object() || !profileData.contains(CoolerKey) || !profileData[CoolerKey].is_object())
    {
        return;
    }

    const Ax120DisplayOptions displayOptions = Ax120DisplayOptions::FromJson(profileData[CoolerKey]);
    QMetaObject::invokeMethod(this, [this, displayOptions] { ApplyDisplayOptions(displayOptions); }, Qt::QueuedConnection);
}

nlohmann::json DiyRgbToolsPlugin::OnProfileSave()
{
    nlohmann::json profileData;
    profileData[CoolerKey] = coolerSettings.display.ToJson();
    return profileData;
}

unsigned char* DiyRgbToolsPlugin::OnSDKCommand(unsigned int, unsigned char*, unsigned int* packetSize)
{
    if(packetSize != nullptr)
    {
        *packetSize = 0;
    }
    return nullptr;
}

void DiyRgbToolsPlugin::ProfileManagerUpdated(unsigned int)
{
}

void DiyRgbToolsPlugin::ResourceManagerUpdated(unsigned int updateReason)
{
    if(updateReason == RESOURCEMANAGER_UPDATE_REASON_DETECTION_COMPLETE)
    {
        QMetaObject::invokeMethod(this, [this] { CreateMissingDevices(); }, Qt::QueuedConnection);
    }
}

void DiyRgbToolsPlugin::SettingsManagerUpdated(unsigned int)
{
}

void DiyRgbToolsPlugin::CreateMissingDevices()
{
    if(pluginApi == nullptr)
    {
        return;
    }

    bool boardTaken = false;
    bool mouseTaken = false;
    FindConflicts(boardTaken, mouseTaken);

    CreateDevice(cooler, CoolerKey, [this]() -> std::unique_ptr<CoolerDevice>
    {
        const std::optional<std::string> path = Ax120HidDevice::FindPath();
        if(!path.has_value())
        {
            return nullptr;
        }
        return std::make_unique<CoolerDevice>(pluginApi, Logger(), *path, coolerSettings, [this]
        {
            QMetaObject::invokeMethod(this, [this]
            {
                if(cooler)
                {
                    coolerSettings.lighting = cooler->Lighting();
                    StoreSetting(CoolerKey, coolerSettings.ToJson());
                }
            }, Qt::QueuedConnection);
        });
    });

    if(boardTaken)
    {
        unavailableReasons[BoardKey] = "OpenRGB's own ASRock Polychrome USB driver is using it";
    }
    else
    {
        CreateDevice(board, BoardKey, [this]() -> std::unique_ptr<BoardDevice>
        {
            const std::optional<std::string> path = BoardDevice::FindPath();
            if(!path.has_value())
            {
                return nullptr;
            }
            return std::make_unique<BoardDevice>(pluginApi, Logger(), *path, pluginApi->GetSettings(BoardDevice::BuiltInDetector));
        });
    }

    if(mouseTaken)
    {
        unavailableReasons[MouseKey] = "OpenRGB's own Logitech driver is using it";
    }
    else
    {
        CreateDevice(mouse, MouseKey, [this]() -> std::unique_ptr<G203Device>
        {
            const std::optional<std::string> path = G203Device::FindPath();
            if(!path.has_value())
            {
                return nullptr;
            }
            return std::make_unique<G203Device>(pluginApi, Logger(), *path, settings.value(MouseKey, nlohmann::json::object()),
                                                [this](const nlohmann::json& saved) { StoreSetting(MouseKey, saved); });
        });
    }

    CreateDevice(keyboard, KeyboardKey, [this]() -> std::unique_ptr<Ak820Device>
    {
        const std::optional<Ak820Interfaces> interfaces = Ak820Interfaces::Find();
        if(!interfaces.has_value())
        {
            return nullptr;
        }
        return std::make_unique<Ak820Device>(pluginApi, Logger(), *interfaces, settings.value(KeyboardKey, nlohmann::json::object()),
                                             [this](const nlohmann::json& saved) { StoreSetting(KeyboardKey, saved); });
    });
}

template<typename Device, typename Factory>
void DiyRgbToolsPlugin::CreateDevice(std::unique_ptr<Device>& slot, const std::string& key, Factory factory)
{
    if(slot)
    {
        return;
    }

    try
    {
        slot = factory();
        unavailableReasons[key] = slot ? "" : "not found";
    }
    catch(const std::exception& error)
    {
        unavailableReasons[key] = std::string("could not start: ") + error.what();
        Log(LL_ERROR, "[DIY RGB Tools] " + key + " could not start: " + error.what());
    }
}

void DiyRgbToolsPlugin::FindConflicts(bool& boardTaken, bool& mouseTaken)
{
    conflictingDrivers.clear();
    bool genericLogitech = false;
    bool legacyLogitech  = false;

    for(RGBControllerInterface* controller : pluginApi->GetRGBControllers())
    {
        if(controller->GetFlags() & CONTROLLER_FLAG_VIRTUAL)
        {
            continue;
        }

        const std::string description = controller->GetDescription();
        const bool        isG203      = controller->GetName().find(G203Device::MarkerInName) != std::string::npos;

        boardTaken      = boardTaken || description == BoardDevice::BuiltInDescription;
        genericLogitech = genericLogitech || (isG203 && description == G203Device::GenericDescription);
        legacyLogitech  = legacyLogitech || (isG203 && description == G203Device::LegacyDescription);
    }

    mouseTaken = genericLogitech || legacyLogitech;

    if(boardTaken)
    {
        conflictingDrivers.push_back(BoardDevice::BuiltInDetector);
    }
    if(genericLogitech)
    {
        conflictingDrivers.push_back(G203Device::GenericDetector);
    }
    if(legacyLogitech)
    {
        conflictingDrivers.push_back(G203Device::LegacyDetector);
    }
}

void DiyRgbToolsPlugin::TurnOffBuiltInDrivers()
{
    if(pluginApi == nullptr || conflictingDrivers.empty())
    {
        return;
    }

    nlohmann::json detectorSettings = pluginApi->GetSettings("Detectors");

    if(!detectorSettings.is_object())
    {
        detectorSettings = nlohmann::json::object();
    }

    for(const std::string& driver : conflictingDrivers)
    {
        detectorSettings["detectors"][driver] = false;
    }

    pluginApi->SetSettings("Detectors", detectorSettings);
    pluginApi->SaveSettings();
    restartNeeded = true;
    Log(LL_INFO, "[DIY RGB Tools] turned off OpenRGB's own drivers; restart OpenRGB to let the plugin take over");
}

DevicesOverview DiyRgbToolsPlugin::Overview() const
{
    DevicesOverview overview;
    overview.conflictingDrivers = conflictingDrivers;
    overview.restartNeeded      = restartNeeded;

    for(const auto& [key, name] : DeviceKeys)
    {
        const VirtualDevice* device = nullptr;

        if(key == BoardKey)
        {
            device = board.get();
        }
        else if(key == MouseKey)
        {
            device = mouse.get();
        }
        else if(key == KeyboardKey)
        {
            device = keyboard.get();
        }
        else
        {
            device = cooler.get();
        }

        overview.rows.push_back({ device ? device->DeviceName() : name, StatusOf(device, key) });
    }

    return overview;
}

void DiyRgbToolsPlugin::ApplyDisplayOptions(const Ax120DisplayOptions& displayOptions)
{
    coolerSettings.display = displayOptions;

    if(cooler)
    {
        cooler->SetDisplayOptions(displayOptions);
    }

    StoreSetting(CoolerKey, coolerSettings.ToJson());
}

void DiyRgbToolsPlugin::StoreSetting(const std::string& key, const nlohmann::json& value)
{
    QMetaObject::invokeMethod(this, [this, key, value]
    {
        settings[key] = value;
        settingsSaveTimer.start();
    }, Qt::QueuedConnection);
}

void DiyRgbToolsPlugin::SaveSettings()
{
    if(pluginApi == nullptr || settings == lastSavedSettings)
    {
        return;
    }

    pluginApi->SetSettings(SettingsKey, settings);
    pluginApi->SaveSettings();
    lastSavedSettings = settings;
}

PluginLog DiyRgbToolsPlugin::Logger() const
{
    OpenRGBPluginAPIInterface* api = pluginApi;
    return [api](unsigned int logLevel, const std::string& message)
    {
        api->LogEntry(__FILE__, __LINE__, logLevel, "%s", message.c_str());
    };
}

void DiyRgbToolsPlugin::Log(unsigned int logLevel, const std::string& message) const
{
    if(pluginApi != nullptr)
    {
        Logger()(logLevel, message);
    }
}

Ax120PluginSettings DiyRgbToolsPlugin::CurrentCoolerSettings() const
{
    Ax120PluginSettings current = coolerSettings;

    if(cooler)
    {
        current.lighting = cooler->Lighting();
    }
    return current;
}

std::string DiyRgbToolsPlugin::StatusOf(const VirtualDevice* device, const std::string& key) const
{
    if(device != nullptr)
    {
        const std::string status = device->StatusText();
        return status.empty() ? "ready" : status;
    }

    const auto reason = unavailableReasons.find(key);
    return reason == unavailableReasons.end() || reason->second.empty() ? "not found" : reason->second;
}
