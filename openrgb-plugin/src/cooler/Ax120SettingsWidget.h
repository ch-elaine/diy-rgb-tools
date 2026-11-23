#pragma once

#include <functional>
#include <optional>
#include <QComboBox>
#include <QLabel>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>
#include "Ax120DisplayDriver.h"
#include "Ax120Settings.h"

using DisplayOptionsProvider = std::function<Ax120DisplayOptions()>;
using DisplayOptionsHandler  = std::function<void(const Ax120DisplayOptions&)>;
using StatusProvider         = std::function<std::optional<Ax120DriverStatus>()>;

class Ax120SettingsWidget : public QWidget
{
    Q_OBJECT

public:
    Ax120SettingsWidget(DisplayOptionsProvider displayOptionsProvider, DisplayOptionsHandler displayOptionsChanged, StatusProvider statusProvider);

private:
    QComboBox*              contentSelector;
    QComboBox*              temperatureUnitSelector;
    QSpinBox*               rotationSecondsInput;
    QLabel*                 connectionLabel;
    QLabel*                 sensorLabel;
    QTimer                  statusRefreshTimer;
    DisplayOptionsProvider  displayOptionsProvider;
    DisplayOptionsHandler   displayOptionsChanged;
    StatusProvider          statusProvider;

    Ax120DisplayOptions     ShownDisplayOptions() const;
    void                    ShowDisplayOptions(const Ax120DisplayOptions& displayOptions);
    void                    EmitDisplayOptions();
    void                    RefreshStatus();

    static QString          FormatReading(const std::optional<double>& reading, const QString& unitSuffix);
};
