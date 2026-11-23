#pragma once

#include <string>
#include <vector>
#include <QImage>
#include <QString>
#include "Hidraw.h"

enum class ScreenFit
{
    Crop,
    Pad,
    Stretch,
};

class KeyboardFrame
{
public:
    QImage  image;
    int     durationMilliseconds = 0;
};

class KeyboardAnimation
{
public:
    static constexpr int            ScreenSize     = 128;
    static constexpr std::size_t    HeaderSize     = 256;
    static constexpr std::size_t    FrameSize      = ScreenSize * ScreenSize * 2;
    static constexpr std::size_t    MaximumFrames  = 30;
    static constexpr int            DelayUnit      = 2;
    static constexpr int            MaximumDelay   = 255 * DelayUnit;
    static constexpr int            DefaultDelay   = 100;
    static constexpr int            MinimumSourceDelay = 20;

    std::vector<KeyboardFrame>  frames;
    std::size_t                 sourceFrameCount = 0;

    static KeyboardAnimation    Load(const QString& path, ScreenFit fit);
    static KeyboardAnimation    FromFrames(const std::vector<KeyboardFrame>& sourceFrames, ScreenFit fit);
    static QImage               Fit(const QImage& source, ScreenFit fit);
    static ByteVector           Rgb565(const QImage& image);

    bool                        IsAnimated() const;
    std::size_t                 ChunkCount() const;
    std::vector<std::string>    Describe() const;
    ByteVector                  Encode() const;
};
