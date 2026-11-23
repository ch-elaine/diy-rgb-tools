#include "DevicesWidget.h"

#include <QVBoxLayout>

namespace
{
    constexpr int RefreshMilliseconds = 1000;
}

DevicesWidget::DevicesWidget(std::function<DevicesOverview()> overviewProviderFunction, std::function<void()> turnOffBuiltInDriversFunction)
    : overviewProvider(std::move(overviewProviderFunction)),
      turnOffBuiltInDrivers(std::move(turnOffBuiltInDriversFunction))
{
    QLabel* introduction = new QLabel("These devices are driven by this plugin. Pick their lighting on the Devices page as usual; "
                                      "\"Save to device\" stores it on the hardware so it survives a restart without OpenRGB. "
                                      "RAM and the Logitech G203 mouse stay on OpenRGB's own drivers.", this);
    introduction->setWordWrap(true);

    statusLabel = new QLabel(this);
    statusLabel->setWordWrap(true);
    statusLabel->setTextFormat(Qt::RichText);

    conflictLabel = new QLabel(this);
    conflictLabel->setWordWrap(true);

    turnOffButton = new QPushButton("Turn off OpenRGB's own drivers for these devices", this);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(introduction);
    layout->addWidget(statusLabel);
    layout->addWidget(conflictLabel);
    layout->addWidget(turnOffButton);
    layout->addStretch();

    connect(turnOffButton, &QPushButton::clicked, this, [this]
    {
        turnOffBuiltInDrivers();
        Refresh();
    });
    connect(&refreshTimer, &QTimer::timeout, this, [this] { Refresh(); });
    refreshTimer.start(RefreshMilliseconds);
    Refresh();
}

void DevicesWidget::Refresh()
{
    const DevicesOverview overview = overviewProvider();
    QString               rows;

    for(const DeviceStatusRow& row : overview.rows)
    {
        rows += "<p><b>" + QString::fromStdString(row.name).toHtmlEscaped() + "</b><br>"
              + QString::fromStdString(row.status).toHtmlEscaped() + "</p>";
    }
    statusLabel->setText(rows);

    if(overview.restartNeeded)
    {
        conflictLabel->setText("OpenRGB's own drivers are turned off. Restart OpenRGB to let this plugin take over.");
        turnOffButton->setVisible(false);
    }
    else if(!overview.conflictingDrivers.empty())
    {
        QString drivers;

        for(const std::string& driver : overview.conflictingDrivers)
        {
            drivers += (drivers.isEmpty() ? "" : ", ") + QString::fromStdString(driver);
        }

        conflictLabel->setText("OpenRGB's own drivers still handle some of these devices (" + drivers + "), so this plugin "
                               "leaves them alone. Turning them off needs an OpenRGB restart.");
        turnOffButton->setVisible(true);
    }
    else
    {
        conflictLabel->clear();
        turnOffButton->setVisible(false);
    }
}
