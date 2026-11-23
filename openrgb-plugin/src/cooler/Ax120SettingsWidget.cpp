#include "Ax120SettingsWidget.h"

#include <cmath>
#include <QFormLayout>
#include <QGroupBox>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace
{
    constexpr int StatusRefreshMilliseconds = 1000;
    constexpr int MinimumRotationSeconds    = 1;
    constexpr int MaximumRotationSeconds    = 60;

    std::optional<double> InUnit(const std::optional<double>& celsius, TemperatureUnit temperatureUnit)
    {
        if(!celsius.has_value() || temperatureUnit == TemperatureUnit::Celsius)
        {
            return celsius;
        }
        return *celsius * 9.0 / 5.0 + 32.0;
    }
}

Ax120SettingsWidget::Ax120SettingsWidget(DisplayOptionsProvider displayOptionsProviderFunction, DisplayOptionsHandler displayOptionsChangedHandler, StatusProvider statusProviderFunction)
    : displayOptionsProvider(std::move(displayOptionsProviderFunction)),
      displayOptionsChanged(std::move(displayOptionsChangedHandler)),
      statusProvider(std::move(statusProviderFunction))
{
    contentSelector = new QComboBox(this);
    contentSelector->addItem("CPU and GPU, rotating",             static_cast<int>(DisplayContent::Rotate));
    contentSelector->addItem("CPU and GPU temperature, rotating", static_cast<int>(DisplayContent::RotateTemperatures));
    contentSelector->addItem("CPU temperature",                   static_cast<int>(DisplayContent::CpuTemperature));
    contentSelector->addItem("CPU usage",                         static_cast<int>(DisplayContent::CpuUsage));
    contentSelector->addItem("GPU temperature",                   static_cast<int>(DisplayContent::GpuTemperature));
    contentSelector->addItem("GPU usage",                         static_cast<int>(DisplayContent::GpuUsage));
    contentSelector->addItem("Every segment lit (lighting only)", static_cast<int>(DisplayContent::AllSegments));

    temperatureUnitSelector = new QComboBox(this);
    temperatureUnitSelector->addItem(QString::fromUtf8("Celsius (°C)"),    static_cast<int>(TemperatureUnit::Celsius));
    temperatureUnitSelector->addItem(QString::fromUtf8("Fahrenheit (°F)"), static_cast<int>(TemperatureUnit::Fahrenheit));

    rotationSecondsInput = new QSpinBox(this);
    rotationSecondsInput->setRange(MinimumRotationSeconds, MaximumRotationSeconds);
    rotationSecondsInput->setSuffix(" s");

    ShowDisplayOptions(displayOptionsProvider());

    connectionLabel = new QLabel(this);
    sensorLabel     = new QLabel(this);

    QLabel* colorHint = new QLabel("Set colors and brightness on the \"Thermalright AX120 Digital\" device in the Devices tab. "
                                   "Each lit segment shows the color you give it there.", this);
    colorHint->setWordWrap(true);

    QFormLayout* optionsLayout = new QFormLayout();
    optionsLayout->addRow("Display shows",    contentSelector);
    optionsLayout->addRow("Temperature unit", temperatureUnitSelector);
    optionsLayout->addRow("Rotate every",     rotationSecondsInput);

    QGroupBox* optionsGroup = new QGroupBox("Digits", this);
    optionsGroup->setLayout(optionsLayout);

    QFormLayout* statusLayout = new QFormLayout();
    statusLayout->addRow("Display", connectionLabel);
    statusLayout->addRow("Sensors", sensorLabel);

    QGroupBox* statusGroup = new QGroupBox("Status", this);
    statusGroup->setLayout(statusLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(optionsGroup);
    mainLayout->addWidget(statusGroup);
    mainLayout->addWidget(colorHint);

    connect(contentSelector,         &QComboBox::currentIndexChanged, this, [this] { EmitDisplayOptions(); });
    connect(temperatureUnitSelector, &QComboBox::currentIndexChanged, this, [this] { EmitDisplayOptions(); });
    connect(rotationSecondsInput,    &QSpinBox::valueChanged,         this, [this] { EmitDisplayOptions(); });
    connect(&statusRefreshTimer,     &QTimer::timeout,                this, [this] { RefreshStatus(); });

    statusRefreshTimer.start(StatusRefreshMilliseconds);
    RefreshStatus();
}

Ax120DisplayOptions Ax120SettingsWidget::ShownDisplayOptions() const
{
    Ax120DisplayOptions displayOptions;
    displayOptions.content         = static_cast<DisplayContent>(contentSelector->currentData().toInt());
    displayOptions.temperatureUnit = static_cast<TemperatureUnit>(temperatureUnitSelector->currentData().toInt());
    displayOptions.rotationSeconds = rotationSecondsInput->value();
    return displayOptions;
}

void Ax120SettingsWidget::ShowDisplayOptions(const Ax120DisplayOptions& displayOptions)
{
    const QSignalBlocker contentBlocker(contentSelector);
    const QSignalBlocker temperatureUnitBlocker(temperatureUnitSelector);
    const QSignalBlocker rotationBlocker(rotationSecondsInput);

    contentSelector->setCurrentIndex(contentSelector->findData(static_cast<int>(displayOptions.content)));
    temperatureUnitSelector->setCurrentIndex(temperatureUnitSelector->findData(static_cast<int>(displayOptions.temperatureUnit)));
    rotationSecondsInput->setValue(displayOptions.rotationSeconds);
    rotationSecondsInput->setEnabled(displayOptions.IsRotating());
}

void Ax120SettingsWidget::EmitDisplayOptions()
{
    const Ax120DisplayOptions displayOptions = ShownDisplayOptions();
    rotationSecondsInput->setEnabled(displayOptions.IsRotating());
    displayOptionsChanged(displayOptions);
    RefreshStatus();
}

void Ax120SettingsWidget::RefreshStatus()
{
    const Ax120DisplayOptions currentDisplayOptions = displayOptionsProvider();

    if(currentDisplayOptions != ShownDisplayOptions())
    {
        ShowDisplayOptions(currentDisplayOptions);
    }

    const std::optional<Ax120DriverStatus> status = statusProvider();

    if(!status.has_value())
    {
        connectionLabel->setText("Not found (USB 0416:8001); close TRCC if it runs, then Rescan Devices");
        sensorLabel->clear();
        return;
    }

    switch(status->connectionState)
    {
        case ConnectionState::Connected:
            connectionLabel->setText(QString("Connected at %1").arg(QString::fromStdString(status->devicePath)));
            break;
        case ConnectionState::Searching:
            connectionLabel->setText("Waiting for the display to become available");
            break;
        case ConnectionState::UnsupportedModel:
            connectionLabel->setText(QString("Unsupported model byte %1, not driven")
                                     .arg(status->productModel.value_or(0)));
            break;
    }

    if(status->connectionState != ConnectionState::Connected)
    {
        sensorLabel->setText("Read once the display is connected");
        return;
    }

    const TemperatureUnit temperatureUnit   = static_cast<TemperatureUnit>(temperatureUnitSelector->currentData().toInt());
    const QString         temperatureSuffix = QString::fromUtf8(temperatureUnit == TemperatureUnit::Fahrenheit ? " °F" : " °C");
    const SensorReadings& readings          = status->sensorReadings;

    sensorLabel->setText(QString::fromUtf8("CPU %1 · %2      GPU %3 · %4")
                         .arg(FormatReading(InUnit(readings.cpuTemperature, temperatureUnit), temperatureSuffix),
                              FormatReading(readings.cpuUsage, " %"),
                              FormatReading(InUnit(readings.gpuTemperature, temperatureUnit), temperatureSuffix),
                              FormatReading(readings.gpuUsage, " %")));
}

QString Ax120SettingsWidget::FormatReading(const std::optional<double>& reading, const QString& unitSuffix)
{
    if(!reading.has_value())
    {
        return "not available";
    }
    return QString::number(std::lround(*reading)) + unitSuffix;
}
