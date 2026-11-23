#include "CoolerServiceWidget.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace
{
    constexpr int RefreshMilliseconds = 5000;
}

CoolerServiceWidget::CoolerServiceWidget(CoolerServiceManager* serviceManager, std::function<Ax120PluginSettings()> settingsProvider)
    : manager(serviceManager),
      currentSettings(std::move(settingsProvider))
{
    stateLabel = new QLabel(this);
    stateLabel->setWordWrap(true);
    messageLabel = new QLabel(this);
    messageLabel->setWordWrap(true);
    installButton = new QPushButton(this);
    removeButton  = new QPushButton("Remove cooler service", this);

    QHBoxLayout* buttonRow = new QHBoxLayout();
    buttonRow->addWidget(installButton);
    buttonRow->addWidget(removeButton);
    buttonRow->addStretch();

    QVBoxLayout* groupLayout = new QVBoxLayout();
    groupLayout->addWidget(stateLabel);
    groupLayout->addLayout(buttonRow);
    groupLayout->addWidget(messageLabel);

    QGroupBox* group = new QGroupBox("Service", this);
    group->setLayout(groupLayout);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(group);

    connect(installButton, &QPushButton::clicked, this, [this] { manager->Install(currentSettings()); });
    connect(removeButton, &QPushButton::clicked, this, [this] { manager->Remove(); });
    connect(manager, &CoolerServiceManager::Changed, this, [this] { ShowState(); });
    connect(&refreshTimer, &QTimer::timeout, this, [this] { manager->Refresh(); });
    refreshTimer.start(RefreshMilliseconds);
    ShowState();
}

void CoolerServiceWidget::ShowState()
{
    const bool busy = manager->IsBusy();

    if(!manager->IsInstalled())
    {
        stateLabel->setText("Not installed, so the display stays dark while OpenRGB is closed. The service starts in early boot "
                            "and drives the display until OpenRGB opens; installing it asks for your password once.");
        installButton->setText("Install cooler service");
        installButton->setVisible(true);
        removeButton->setVisible(false);
    }
    else
    {
        const QString activity = manager->Activity().isEmpty() ? QString("unknown") : manager->Activity();
        stateLabel->setText(QString("Installed (%1). It starts in early boot and drives the display while OpenRGB is closed; "
                                    "Save to device on the cooler changes what it shows.%2")
                            .arg(activity, manager->NeedsUpdate() ? " This plugin carries a newer version of it." : ""));
        installButton->setText("Update cooler service");
        installButton->setVisible(manager->NeedsUpdate());
        removeButton->setVisible(true);
    }

    installButton->setEnabled(!busy);
    removeButton->setEnabled(!busy);
    messageLabel->setText(manager->Message());
    messageLabel->setVisible(!manager->Message().isEmpty());
}
