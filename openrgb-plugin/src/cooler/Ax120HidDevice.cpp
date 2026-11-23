#include "Ax120HidDevice.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <thread>

namespace
{
    constexpr std::size_t                     ReportSize          = 64;
    constexpr std::size_t                     HeaderSize          = 20;
    constexpr std::array<unsigned char, 4>    Magic               = { 0xDA, 0xDB, 0xDC, 0xDD };
    constexpr std::size_t                     CommandOffset       = 12;
    constexpr std::size_t                     PayloadLengthOffset = 16;
    constexpr std::size_t                     ProductModelOffset  = 5;
    constexpr unsigned char                   InitCommand         = 0x01;
    constexpr unsigned char                   ColorCommand        = 0x02;
    constexpr std::array<unsigned char, 3>    SupportedProductModels = { 1, 2, 3 };
    constexpr std::chrono::milliseconds       DelayBeforeHandshake{ 50 };
    constexpr std::chrono::milliseconds       DelayAfterHandshake{ 200 };
    constexpr int                             HandshakeReplyTimeoutMilliseconds = 500;
}

Ax120HidDevice::~Ax120HidDevice()
{
    Close();
}

std::optional<std::string> Ax120HidDevice::FindPath()
{
    const std::vector<HidrawNode> nodes = HidrawNode::Find(VendorId, ProductId);

    if(nodes.empty())
    {
        return std::nullopt;
    }
    return nodes.front().devicePath;
}

bool Ax120HidDevice::Open()
{
    Close();

    const std::optional<std::string> foundPath = FindPath();

    if(!foundPath.has_value())
    {
        return false;
    }

    try
    {
        handle = std::make_unique<HidrawHandle>(*foundPath);
    }
    catch(const DeviceError&)
    {
        return false;
    }

    path = *foundPath;
    return true;
}

void Ax120HidDevice::Close()
{
    handle.reset();

    productModel.reset();
}

bool Ax120HidDevice::IsOpen() const
{
    return handle != nullptr;
}

const std::string& Ax120HidDevice::Path() const
{
    return path;
}

std::optional<unsigned char> Ax120HidDevice::ProductModel() const
{
    return productModel;
}

HandshakeResult Ax120HidDevice::Handshake()
{
    std::array<unsigned char, ReportSize> initReport{};
    std::copy(Magic.begin(), Magic.end(), initReport.begin());
    initReport[CommandOffset] = InitCommand;

    std::this_thread::sleep_for(DelayBeforeHandshake);

    if(!WriteReport(initReport.data(), initReport.size()))
    {
        return HandshakeResult::NoReply;
    }

    std::this_thread::sleep_for(DelayAfterHandshake);

    std::optional<ByteVector> reply;

    try
    {
        reply = handle->Read(std::chrono::milliseconds(HandshakeReplyTimeoutMilliseconds));
    }
    catch(const DeviceError&)
    {
        return HandshakeResult::NoReply;
    }

    const bool isHandshakeReply = reply.has_value()
                               && reply->size() > CommandOffset
                               && std::equal(Magic.begin(), Magic.end(), reply->begin())
                               && (*reply)[CommandOffset] == InitCommand;

    if(!isHandshakeReply)
    {
        return HandshakeResult::NoReply;
    }

    productModel = (*reply)[ProductModelOffset];

    const bool isSupported = std::find(SupportedProductModels.begin(), SupportedProductModels.end(), *productModel) != SupportedProductModels.end();
    return isSupported ? HandshakeResult::SupportedModel : HandshakeResult::UnsupportedModel;
}

bool Ax120HidDevice::SendColors(const std::vector<unsigned char>& rgbPayload)
{
    std::vector<unsigned char> packet(HeaderSize, 0);
    std::copy(Magic.begin(), Magic.end(), packet.begin());
    packet[CommandOffset]           = ColorCommand;
    packet[PayloadLengthOffset]     = static_cast<unsigned char>(rgbPayload.size() & 0xFF);
    packet[PayloadLengthOffset + 1] = static_cast<unsigned char>(rgbPayload.size() >> 8);
    packet.insert(packet.end(), rgbPayload.begin(), rgbPayload.end());

    for(std::size_t reportOffset = 0; reportOffset < packet.size(); reportOffset += ReportSize)
    {
        std::array<unsigned char, ReportSize> report{};
        const std::size_t chunkLength = std::min(ReportSize, packet.size() - reportOffset);
        std::copy_n(packet.begin() + static_cast<std::ptrdiff_t>(reportOffset), chunkLength, report.begin());

        if(!WriteReport(report.data(), report.size()))
        {
            return false;
        }
    }

    return true;
}

bool Ax120HidDevice::WriteReport(const unsigned char* reportData, std::size_t reportLength)
{
    if(handle == nullptr)
    {
        return false;
    }

    ByteVector reportWithIdentifier(reportLength + 1, 0);
    std::copy_n(reportData, reportLength, reportWithIdentifier.begin() + 1);

    try
    {
        handle->Write(reportWithIdentifier);
    }
    catch(const DeviceError&)
    {
        return false;
    }
    return true;
}
