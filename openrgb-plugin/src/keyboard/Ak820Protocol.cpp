#include "Ak820Protocol.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

namespace
{
    constexpr std::chrono::milliseconds SendDelay{ 35 };
    constexpr std::chrono::milliseconds ReplyDelay{ 50 };
    constexpr std::chrono::milliseconds StartRetryDelay{ 300 };
    constexpr std::chrono::milliseconds AcknowledgementTimeout{ 1000 };
    constexpr std::chrono::milliseconds UploadSettleTime{ 100 };
    constexpr int                       StartAttempts = 10;

    constexpr unsigned char StartCommand    = 0x18;
    constexpr unsigned char LightingCommand = 0x13;
    constexpr unsigned char ClockCommand    = 0x28;
    constexpr unsigned char ImageCommand    = 0x72;
    constexpr unsigned char CommitCommand   = 0x02;
    constexpr unsigned char FinishCommand   = 0xF0;
    constexpr unsigned int  SettingsFlag    = 1;
    constexpr unsigned int  UploadFlag      = 0;

    constexpr unsigned short VendorId = 0x0C45;
    const std::vector<unsigned short> ProductIds = { 0x8009, 0x800A };

    const ByteVector ControlDescriptor =
    {
        0x06, 0x13, 0xff, 0x09, 0x01, 0xa1, 0x01, 0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x40,
        0x09, 0x02, 0x81, 0x02, 0x09, 0x03, 0x91, 0x02, 0x0a, 0x00, 0xff, 0xb1, 0x02, 0xc0,
    };
    const ByteVector DisplayDescriptor =
    {
        0x06, 0x68, 0xff, 0x09, 0x61, 0xa1, 0x01, 0x09, 0x62, 0x15, 0x00, 0x26, 0xff, 0x00, 0x95, 0x40,
        0x75, 0x08, 0x81, 0x02, 0x09, 0x63, 0x15, 0x00, 0x26, 0xff, 0x00, 0x96, 0x00, 0x10, 0x75, 0x08,
        0x91, 0x02, 0xc0,
    };

    std::string Hex(const ByteVector& bytes, std::size_t count)
    {
        std::string text;
        char        part[4];

        for(std::size_t index = 0; index < std::min(count, bytes.size()); index++)
        {
            std::snprintf(part, sizeof(part), "%02x ", bytes[index]);
            text += part;
        }
        return text.empty() ? text : text.substr(0, text.size() - 1);
    }
}

ByteVector KeyboardLighting::Packet() const
{
    ByteVector packet(Ak820Protocol::ReportSize, 0);
    packet[0]  = modeCode;
    packet[1]  = red;
    packet[2]  = green;
    packet[3]  = blue;
    packet[8]  = rainbow ? 1 : 0;
    packet[9]  = brightness;
    packet[10] = speed;
    packet[11] = direction;
    packet[14] = 0x55;
    packet[15] = 0xAA;
    return packet;
}

std::optional<Ak820Interfaces> Ak820Interfaces::Find()
{
    std::vector<HidrawNode> nodes;

    for(unsigned short productId : ProductIds)
    {
        const std::vector<HidrawNode> found = HidrawNode::Find(VendorId, productId);
        nodes.insert(nodes.end(), found.begin(), found.end());
    }

    for(const HidrawNode& candidate : nodes)
    {
        Ak820Interfaces interfaces;

        for(const HidrawNode& node : nodes)
        {
            if(node.UsbDevice() != candidate.UsbDevice())
            {
                continue;
            }
            if(node.reportDescriptor == ControlDescriptor)
            {
                interfaces.controlPath = node.devicePath;
            }
            if(node.reportDescriptor == DisplayDescriptor)
            {
                interfaces.displayPath = node.devicePath;
            }
        }

        if(!interfaces.controlPath.empty() && !interfaces.displayPath.empty())
        {
            return interfaces;
        }
    }

    return std::nullopt;
}

Ak820Protocol::Ak820Protocol(HidTransport& controlTransport)
    : control(controlTransport)
{
}

void Ak820Protocol::SetLighting(const KeyboardLighting& lighting)
{
    WriteSetting(LightingCommand, { [lighting] { return lighting.Packet(); } });
}

std::tm Ak820Protocol::SyncClock()
{
    std::tm moment{};

    WriteSetting(ClockCommand, { [&moment]
    {
        const auto   now        = std::chrono::system_clock::now();
        const double nowSeconds = std::chrono::duration<double>(now.time_since_epoch()).count();
        double       nextSecond = std::floor(nowSeconds) + 1;
        const double leadTime   = std::chrono::duration<double>(SendDelay).count();

        if(nextSecond - nowSeconds < leadTime)
        {
            nextSecond += 1;
        }

        std::this_thread::sleep_for(std::chrono::duration<double>(nextSecond - leadTime - nowSeconds));
        const std::time_t nextTime = static_cast<std::time_t>(nextSecond);
        localtime_r(&nextTime, &moment);
        return ClockPacket(moment);
    } });

    return moment;
}

void Ak820Protocol::UploadScreen(HidTransport& displayTransport, const ByteVector& payload, unsigned int slot,
                                 const std::function<void(std::size_t, std::size_t)>& progress)
{
    const std::size_t chunkCount = payload.size() / ScreenChunkSize;
    displayTransport.Drain();
    StartSession(UploadFlag);

    try
    {
        SendCommand(ImageCommand, static_cast<unsigned char>(slot), static_cast<unsigned int>(chunkCount));

        for(std::size_t chunkIndex = 0; chunkIndex < chunkCount; chunkIndex++)
        {
            ByteVector report(ScreenChunkSize + 1, 0);
            std::copy_n(payload.begin() + static_cast<std::ptrdiff_t>(chunkIndex * ScreenChunkSize), ScreenChunkSize, report.begin() + 1);
            displayTransport.Write(report);

            if(!displayTransport.Read(AcknowledgementTimeout).has_value())
            {
                throw DeviceError("no acknowledgement for screen chunk " + std::to_string(chunkIndex + 1) + "/"
                                  + std::to_string(chunkCount) + "; upload stopped");
            }

            progress(chunkIndex + 1, chunkCount);
        }

        SendCommand(CommitCommand);
    }
    catch(...)
    {
        AbandonSession();
        throw;
    }

    std::this_thread::sleep_for(UploadSettleTime);
}

ByteVector Ak820Protocol::CommandPacket(unsigned char command, unsigned char subcommand, unsigned int value)
{
    ByteVector packet(ReportSize, 0);
    packet[0] = 0x04;
    packet[1] = command;
    packet[2] = subcommand;
    packet[8] = static_cast<unsigned char>(value & 0xFF);
    packet[9] = static_cast<unsigned char>(value >> 8 & 0xFF);
    return packet;
}

ByteVector Ak820Protocol::ClockPacket(const std::tm& moment)
{
    const int year = moment.tm_year + 1900;

    if(year < 2000 || year > 2099)
    {
        throw DeviceError("the keyboard clock cannot hold the year " + std::to_string(year));
    }

    ByteVector packet(ReportSize, 0);
    packet[1]  = 0x01;
    packet[2]  = 0x5A;
    packet[3]  = static_cast<unsigned char>(year - 2000);
    packet[4]  = static_cast<unsigned char>(moment.tm_mon + 1);
    packet[5]  = static_cast<unsigned char>(moment.tm_mday);
    packet[6]  = static_cast<unsigned char>(moment.tm_hour);
    packet[7]  = static_cast<unsigned char>(moment.tm_min);
    packet[8]  = static_cast<unsigned char>(moment.tm_sec);
    packet[10] = 0x04;
    packet[62] = 0xAA;
    packet[63] = 0x55;
    return packet;
}

ByteVector Ak820Protocol::Exchange(const ByteVector& packet)
{
    ByteVector report{ 0 };
    report.insert(report.end(), packet.begin(), packet.end());

    std::this_thread::sleep_for(SendDelay);
    control.SendFeature(report);
    std::this_thread::sleep_for(ReplyDelay);

    const ByteVector reply = control.GetFeature(ReportSize + 1);

    if(reply.size() != ReportSize + 1)
    {
        throw DeviceError("the keyboard sent a short feature reply");
    }
    return ByteVector(reply.begin() + 1, reply.end());
}

void Ak820Protocol::CheckReply(const ByteVector& reply, unsigned char command) const
{
    if(reply[0] != 0x04 || reply[1] != command || reply[3] != 0x01)
    {
        char commandText[8];
        std::snprintf(commandText, sizeof(commandText), "0x%02x", command);
        throw DeviceError(std::string("the keyboard refused command ") + commandText + " (reply " + Hex(reply, 9) + ")");
    }
}

void Ak820Protocol::SendCommand(unsigned char command, unsigned char subcommand, unsigned int value)
{
    CheckReply(Exchange(CommandPacket(command, subcommand, value)), command);
}

void Ak820Protocol::StartSession(unsigned int flag)
{
    const ByteVector busyReply{ 0x04, StartCommand, 0x00, 0xFF };
    const ByteVector startPacket = CommandPacket(StartCommand, 0, flag);

    for(int attempt = 0; attempt < StartAttempts - 1; attempt++)
    {
        const ByteVector reply    = Exchange(startPacket);
        const bool       anyData  = std::any_of(reply.begin(), reply.end(), [](unsigned char value) { return value != 0; });
        const bool       isBusy   = std::equal(busyReply.begin(), busyReply.end(), reply.begin());

        if(anyData && !isBusy)
        {
            CheckReply(reply, StartCommand);
            return;
        }
        std::this_thread::sleep_for(StartRetryDelay);
    }

    CheckReply(Exchange(startPacket), StartCommand);
}

void Ak820Protocol::FinishSession()
{
    SendCommand(FinishCommand, 0, SettingsFlag);
}

void Ak820Protocol::AbandonSession()
{
    try
    {
        FinishSession();
    }
    catch(const std::exception&)
    {
    }
}

void Ak820Protocol::WriteSetting(unsigned char command, const std::vector<std::function<ByteVector()>>& packetProducers)
{
    try
    {
        StartSession(SettingsFlag);
        SendCommand(command, 0, 1);

        for(const std::function<ByteVector()>& producePacket : packetProducers)
        {
            Exchange(producePacket());
        }

        SendCommand(CommitCommand);
    }
    catch(...)
    {
        AbandonSession();
        throw;
    }

    FinishSession();
}
