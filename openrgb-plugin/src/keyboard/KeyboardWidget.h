#pragma once

#include <functional>
#include <optional>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>
#include "Ak820Device.h"
#include "KeyboardAnimation.h"

class KeyboardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit KeyboardWidget(std::function<Ak820Device*()> keyboardProvider);

private:
    std::function<Ak820Device*()>       keyboardProvider;
    std::optional<KeyboardAnimation>    animation;
    QString                             imagePath;
    QLabel*                             availabilityLabel;
    QPushButton*                        chooseButton;
    QLabel*                             pathLabel;
    QComboBox*                          fitSelector;
    QSpinBox*                           slotInput;
    QLabel*                             previewLabel;
    QLabel*                             detailsLabel;
    QPushButton*                        uploadButton;
    QPushButton*                        clockButton;
    QProgressBar*                       progressBar;
    QLabel*                             taskLabel;
    QTimer                              refreshTimer;

    void                                ChooseImage();
    void                                PrepareAnimation();
    void                                Upload();
    void                                SyncClock();
    void                                Refresh();
};
