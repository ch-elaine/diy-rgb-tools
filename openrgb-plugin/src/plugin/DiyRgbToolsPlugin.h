#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <QObject>
#include <QTimer>
#include "OpenRGBPluginInterface.h"
#include "Ak820Device.h"
#include "Ax120Settings.h"
#include "BoardDevice.h"
#include "CoolerDevice.h"
#include "CoolerServiceManager.h"
#include "DevicesWidget.h"
#include "G203Device.h"

class DiyRgbToolsPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "DiyRgbToolsPlugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    DiyRgbToolsPlugin();
    ~DiyRgbToolsPlugin() override;

    OpenRGBPluginInfo   GetPluginInfo() override;
    unsigned int        GetPluginAPIVersion() override;

    void                Load(OpenRGBPluginAPIInterface* pluginApiPointer) override;
    QWidget*            GetWidget() override;
    QMenu*              GetTrayMenu() override;
    void                Unload() override;
    void                OnProfileAboutToLoad() override;
    void                OnProfileLoad(nlohmann::json profileData) override;
    nlohmann::json      OnProfileSave() override;
    unsigned char*      OnSDKCommand(unsigned int packetId, unsigned char* packetData, unsigned int* packetSize) override;

    void                ProfileManagerUpdated(unsigned int updateReason) override;
    void                ResourceManagerUpdated(unsigned int updateReason) override;
    void                SettingsManagerUpdated(unsigned int updateReason) override;

private:
    OpenRGBPluginAPIInterface*          pluginApi = nullptr;
    nlohmann::json                      settings;
    nlohmann::json                      lastSavedSettings;
    Ax120PluginSettings                 coolerSettings;
    std::unique_ptr<CoolerDevice>       cooler;
    std::unique_ptr<BoardDevice>        board;
    std::unique_ptr<G203Device>         mouse;
    std::unique_ptr<Ak820Device>        keyboard;
    std::map<std::string, std::string>  unavailableReasons;
    std::vector<std::string>            conflictingDrivers;
    bool                                restartNeeded = false;
    QTimer                              settingsSaveTimer;
    CoolerServiceManager*               serviceManager = nullptr;

    void                CreateMissingDevices();
    void                FindConflicts(bool& boardTaken, bool& mouseTaken);
    void                TurnOffBuiltInDrivers();
    DevicesOverview     Overview() const;
    void                ApplyDisplayOptions(const Ax120DisplayOptions& displayOptions);
    void                StoreSetting(const std::string& key, const nlohmann::json& value);
    void                SaveSettings();
    PluginLog           Logger() const;
    void                Log(unsigned int logLevel, const std::string& message) const;
    std::string         StatusOf(const VirtualDevice* device, const std::string& key) const;
    Ax120PluginSettings CurrentCoolerSettings() const;

    template<typename Device, typename Factory>
    void                CreateDevice(std::unique_ptr<Device>& slot, const std::string& key, Factory factory);
};
