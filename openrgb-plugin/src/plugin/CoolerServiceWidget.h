#pragma once

#include <functional>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QWidget>
#include "Ax120Settings.h"
#include "CoolerServiceManager.h"

class CoolerServiceWidget : public QWidget
{
    Q_OBJECT

public:
    CoolerServiceWidget(CoolerServiceManager* serviceManager, std::function<Ax120PluginSettings()> settingsProvider);

private:
    CoolerServiceManager*                   manager;
    std::function<Ax120PluginSettings()>    currentSettings;
    QLabel*                                 stateLabel;
    QLabel*                                 messageLabel;
    QPushButton*                            installButton;
    QPushButton*                            removeButton;
    QTimer                                  refreshTimer;

    void                                    ShowState();
};
