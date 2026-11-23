#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Hidraw.h"

enum class HandshakeResult
{
    SupportedModel,
    NoReply,
    UnsupportedModel,
};

class Ax120HidDevice
{
public:
    static constexpr unsigned short VendorId  = 0x0416;
    static constexpr unsigned short ProductId = 0x8001;

    Ax120HidDevice() = default;
    Ax120HidDevice(const Ax120HidDevice&) = delete;
    Ax120HidDevice& operator=(const Ax120HidDevice&) = delete;
    ~Ax120HidDevice();

    static std::optional<std::string>   FindPath();

    bool                                Open();
    void                                Close();
    bool                                IsOpen() const;
    const std::string&                  Path() const;
    HandshakeResult                     Handshake();
    std::optional<unsigned char>        ProductModel() const;
    bool                                SendColors(const std::vector<unsigned char>& rgbPayload);

private:
    std::unique_ptr<HidrawHandle>       handle;
    std::string                         path;
    std::optional<unsigned char>        productModel;

    bool                                WriteReport(const unsigned char* reportData, std::size_t reportLength);
};
