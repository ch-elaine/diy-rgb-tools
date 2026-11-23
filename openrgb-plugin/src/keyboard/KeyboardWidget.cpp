#include "KeyboardWidget.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPixmap>
#include <QVBoxLayout>

namespace
{
    constexpr int RefreshMilliseconds = 250;
}

KeyboardWidget::KeyboardWidget(std::function<Ak820Device*()> keyboardProviderFunction)
    : keyboardProvider(std::move(keyboardProviderFunction))
{
    availabilityLabel = new QLabel(this);
    availabilityLabel->setWordWrap(true);

    QLabel* lightingHint = new QLabel("Lighting: pick a mode for \"Ajazz AK820 Pro\" on the Devices page and press Save to device. "
                                      "The keyboard keeps every write in its flash, so nothing is sent until you save.", this);
    lightingHint->setWordWrap(true);

    chooseButton = new QPushButton(QString::fromUtf8("Choose image…"), this);
    pathLabel    = new QLabel("No image chosen", this);

    fitSelector = new QComboBox(this);
    fitSelector->addItem("Crop to fill the screen", static_cast<int>(ScreenFit::Crop));
    fitSelector->addItem("Pad with black",          static_cast<int>(ScreenFit::Pad));
    fitSelector->addItem("Stretch",                 static_cast<int>(ScreenFit::Stretch));

    slotInput = new QSpinBox(this);
    slotInput->setRange(1, 3);

    previewLabel = new QLabel(this);
    previewLabel->setFixedSize(KeyboardAnimation::ScreenSize + 2, KeyboardAnimation::ScreenSize + 2);
    previewLabel->setFrameShape(QFrame::Box);
    previewLabel->setAlignment(Qt::AlignCenter);

    detailsLabel = new QLabel(this);
    detailsLabel->setWordWrap(true);

    uploadButton = new QPushButton("Upload to the keyboard", this);
    progressBar  = new QProgressBar(this);
    clockButton  = new QPushButton("Set the keyboard clock to this computer's time", this);
    taskLabel    = new QLabel(this);
    taskLabel->setWordWrap(true);

    QHBoxLayout* chooseRow = new QHBoxLayout();
    chooseRow->addWidget(chooseButton);
    chooseRow->addWidget(pathLabel, 1);

    QFormLayout* screenForm = new QFormLayout();
    screenForm->addRow("Image", chooseRow);
    screenForm->addRow("Fit",   fitSelector);
    screenForm->addRow("Slot",  slotInput);

    QHBoxLayout* uploadRow = new QHBoxLayout();
    uploadRow->addWidget(uploadButton);
    uploadRow->addWidget(progressBar, 1);

    QVBoxLayout* settingsColumn = new QVBoxLayout();
    settingsColumn->addLayout(screenForm);
    settingsColumn->addWidget(detailsLabel);
    settingsColumn->addStretch();

    QHBoxLayout* settingsRow = new QHBoxLayout();
    settingsRow->addLayout(settingsColumn, 1);
    settingsRow->addWidget(previewLabel, 0, Qt::AlignTop);

    QVBoxLayout* screenLayout = new QVBoxLayout();
    screenLayout->addLayout(settingsRow);
    screenLayout->addLayout(uploadRow);

    QGroupBox* screenGroup = new QGroupBox(QString::fromUtf8("Screen (GIF, PNG or JPEG, resized to 128×128)"), this);
    screenGroup->setLayout(screenLayout);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(availabilityLabel);
    layout->addWidget(lightingHint);
    layout->addWidget(screenGroup);
    layout->addWidget(clockButton);
    layout->addWidget(taskLabel);
    layout->addStretch();

    connect(chooseButton, &QPushButton::clicked, this, [this] { ChooseImage(); });
    connect(fitSelector, &QComboBox::currentIndexChanged, this, [this] { PrepareAnimation(); });
    connect(uploadButton, &QPushButton::clicked, this, [this] { Upload(); });
    connect(clockButton, &QPushButton::clicked, this, [this] { SyncClock(); });
    connect(&refreshTimer, &QTimer::timeout, this, [this] { Refresh(); });
    refreshTimer.start(RefreshMilliseconds);
    Refresh();
}

void KeyboardWidget::ChooseImage()
{
    const QString chosen = QFileDialog::getOpenFileName(this, "Choose a screen image", imagePath,
                                                        "Images (*.gif *.png *.jpg *.jpeg)");
    if(chosen.isEmpty())
    {
        return;
    }

    imagePath = chosen;
    pathLabel->setText(QFileInfo(chosen).fileName());
    pathLabel->setToolTip(chosen);
    PrepareAnimation();
}

void KeyboardWidget::PrepareAnimation()
{
    if(imagePath.isEmpty())
    {
        return;
    }

    try
    {
        animation = KeyboardAnimation::Load(imagePath, static_cast<ScreenFit>(fitSelector->currentData().toInt()));
        previewLabel->setPixmap(QPixmap::fromImage(animation->frames.front().image)
                                .scaled(KeyboardAnimation::ScreenSize, KeyboardAnimation::ScreenSize));

        QString details;

        for(const std::string& line : animation->Describe())
        {
            details += (details.isEmpty() ? "" : "\n") + QString::fromStdString(line);
        }
        detailsLabel->setText(details);
    }
    catch(const std::exception& error)
    {
        animation.reset();
        previewLabel->clear();
        detailsLabel->setText(error.what());
    }

    Refresh();
}

void KeyboardWidget::Upload()
{
    Ak820Device* keyboard = keyboardProvider();

    if(keyboard == nullptr || !animation.has_value())
    {
        return;
    }

    try
    {
        keyboard->StartScreenUpload(*animation, static_cast<unsigned int>(slotInput->value()));
    }
    catch(const std::exception& error)
    {
        taskLabel->setText(error.what());
    }
    Refresh();
}

void KeyboardWidget::SyncClock()
{
    Ak820Device* keyboard = keyboardProvider();

    if(keyboard == nullptr)
    {
        return;
    }

    try
    {
        keyboard->StartClockSync();
    }
    catch(const std::exception& error)
    {
        taskLabel->setText(error.what());
    }
    Refresh();
}

void KeyboardWidget::Refresh()
{
    Ak820Device* keyboard = keyboardProvider();

    if(keyboard == nullptr)
    {
        availabilityLabel->setText("Ajazz AK820 Pro not found (USB 0c45:8009, connected by cable).");
        uploadButton->setEnabled(false);
        clockButton->setEnabled(false);
        return;
    }

    const KeyboardTaskProgress progress = keyboard->TaskProgress();
    availabilityLabel->setText("Ajazz AK820 Pro connected.");
    uploadButton->setEnabled(!progress.running && animation.has_value());
    clockButton->setEnabled(!progress.running);
    chooseButton->setEnabled(!progress.running);
    progressBar->setRange(0, static_cast<int>(std::max<std::size_t>(progress.total, 1)));
    progressBar->setValue(static_cast<int>(progress.done));
    taskLabel->setText(QString::fromStdString(progress.message));
}
