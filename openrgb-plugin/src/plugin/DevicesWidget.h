#pragma once

#include <functional>
#include <string>
#include <vector>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

class DeviceStatusRow
{
public:
    std::string name;
    std::string status;
};

class DevicesOverview
{
public:
    std::vector<DeviceStatusRow>    rows;
    std::vector<std::string>        conflictingDrivers;
    bool                            restartNeeded = false;
};

class DevicesWidget : public QWidget
{
    Q_OBJECT

public:
    DevicesWidget(std::function<DevicesOverview()> overviewProvider, std::function<void()> turnOffBuiltInDrivers);

private:
    QLabel*                             statusLabel;
    QLabel*                             conflictLabel;
    QPushButton*                        turnOffButton;
    QTimer                              refreshTimer;
    std::function<DevicesOverview()>    overviewProvider;
    std::function<void()>               turnOffBuiltInDrivers;

    void                                Refresh();
};
