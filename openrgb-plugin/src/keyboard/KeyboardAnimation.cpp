#include "KeyboardAnimation.h"

#include <algorithm>
#include <QImageReader>
#include <QPainter>
#include "Ak820Protocol.h"

KeyboardAnimation KeyboardAnimation::Load(const QString& path, ScreenFit fit)
{
    QImageReader reader(path);
    reader.setAutoTransform(false);
    std::vector<KeyboardFrame> sourceFrames;

    while(true)
    {
        QImage image = reader.read();

        if(image.isNull())
        {
            break;
        }

        const int delay = reader.nextImageDelay();
        sourceFrames.push_back({ image, delay >= MinimumSourceDelay ? delay : DefaultDelay });

        if(!reader.supportsAnimation())
        {
            break;
        }
    }

    if(sourceFrames.empty())
    {
        throw DeviceError("cannot read " + path.toStdString() + ": " + reader.errorString().toStdString());
    }

    return FromFrames(sourceFrames, fit);
}

KeyboardAnimation KeyboardAnimation::FromFrames(const std::vector<KeyboardFrame>& sourceFrames, ScreenFit fit)
{
    KeyboardAnimation animation;
    animation.sourceFrameCount = sourceFrames.size();
    const std::size_t keptFrameCount = std::min(animation.sourceFrameCount, MaximumFrames);

    for(std::size_t index = 0; index < sourceFrames.size(); index++)
    {
        if(index * keptFrameCount / animation.sourceFrameCount == animation.frames.size())
        {
            animation.frames.push_back({ Fit(sourceFrames[index].image, fit), sourceFrames[index].durationMilliseconds });
        }
        else
        {
            animation.frames.back().durationMilliseconds += sourceFrames[index].durationMilliseconds;
        }
    }

    return animation;
}

QImage KeyboardAnimation::Fit(const QImage& source, ScreenFit fit)
{
    QImage flattened(source.size(), QImage::Format_RGB888);
    flattened.fill(Qt::black);
    {
        QPainter painter(&flattened);
        painter.drawImage(0, 0, source);
    }

    const QSize screen(ScreenSize, ScreenSize);

    if(fit == ScreenFit::Stretch)
    {
        return flattened.scaled(screen, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB888);
    }

    if(fit == ScreenFit::Crop)
    {
        const QImage scaled = flattened.scaled(screen, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const int    left   = (scaled.width() - ScreenSize) / 2;
        const int    top    = (scaled.height() - ScreenSize) / 2;
        return scaled.copy(left, top, ScreenSize, ScreenSize).convertToFormat(QImage::Format_RGB888);
    }

    const QImage scaled = flattened.scaled(screen, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QImage       padded(screen, QImage::Format_RGB888);
    padded.fill(Qt::black);
    {
        QPainter painter(&padded);
        painter.drawImage((ScreenSize - scaled.width()) / 2, (ScreenSize - scaled.height()) / 2, scaled);
    }
    return padded;
}

ByteVector KeyboardAnimation::Rgb565(const QImage& image)
{
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    ByteVector   pixels;
    pixels.reserve(static_cast<std::size_t>(rgb.width() * rgb.height() * 2));

    for(int row = 0; row < rgb.height(); row++)
    {
        const uchar* line = rgb.constScanLine(row);

        for(int column = 0; column < rgb.width(); column++)
        {
            const unsigned int red   = line[column * 3];
            const unsigned int green = line[column * 3 + 1];
            const unsigned int blue  = line[column * 3 + 2];
            const unsigned int pixel = (red >> 3) << 11 | (green >> 2) << 5 | blue >> 3;
            pixels.push_back(static_cast<unsigned char>(pixel & 0xFF));
            pixels.push_back(static_cast<unsigned char>(pixel >> 8));
        }
    }

    return pixels;
}

bool KeyboardAnimation::IsAnimated() const
{
    return frames.size() > 1;
}

std::size_t KeyboardAnimation::ChunkCount() const
{
    const std::size_t size = HeaderSize + frames.size() * FrameSize;
    return (size + Ak820Protocol::ScreenChunkSize - 1) / Ak820Protocol::ScreenChunkSize;
}

std::vector<std::string> KeyboardAnimation::Describe() const
{
    std::vector<std::string> lines;
    lines.push_back(std::to_string(frames.size()) + (IsAnimated() ? " frames" : " frame") + ", "
                    + std::to_string(ScreenSize) + "x" + std::to_string(ScreenSize) + ", "
                    + std::to_string(ChunkCount()) + " chunks to send");

    if(sourceFrameCount > frames.size())
    {
        lines.push_back("the source has " + std::to_string(sourceFrameCount) + " frames and the keyboard holds "
                        + std::to_string(MaximumFrames) + ", so frames were dropped evenly and the timing kept");
    }

    const bool hasLongFrame = std::any_of(frames.begin(), frames.end(),
                                          [](const KeyboardFrame& frame) { return frame.durationMilliseconds > MaximumDelay; });

    if(IsAnimated() && hasLongFrame)
    {
        lines.push_back("frames longer than " + std::to_string(MaximumDelay) + " ms are shortened to "
                        + std::to_string(MaximumDelay) + " ms");
    }

    return lines;
}

ByteVector KeyboardAnimation::Encode() const
{
    ByteVector payload(HeaderSize, 0xFF);
    payload[0] = static_cast<unsigned char>(frames.size());

    if(IsAnimated())
    {
        for(std::size_t index = 0; index < frames.size(); index++)
        {
            payload[index + 1] = static_cast<unsigned char>(std::clamp(frames[index].durationMilliseconds / DelayUnit, 1, 255));
        }
    }
    else
    {
        payload[1] = 0;
    }

    for(const KeyboardFrame& frame : frames)
    {
        const ByteVector pixels = Rgb565(frame.image);
        payload.insert(payload.end(), pixels.begin(), pixels.end());
    }

    const std::size_t remainder = payload.size() % Ak820Protocol::ScreenChunkSize;

    if(remainder != 0)
    {
        payload.resize(payload.size() + Ak820Protocol::ScreenChunkSize - remainder, 0xFF);
    }

    return payload;
}
