#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using ByteVector = std::vector<unsigned char>;

class DeviceError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class HidTransport
{
public:
    virtual ~HidTransport() = default;

    virtual const std::string&          Path() const = 0;
    virtual void                        Write(const ByteVector& report) = 0;
    virtual std::optional<ByteVector>   Read(std::chrono::milliseconds timeout) = 0;
    virtual void                        SendFeature(const ByteVector& report) = 0;
    virtual ByteVector                  GetFeature(std::size_t length) = 0;
    virtual void                        LockExclusive();

    void                                Drain();
};

using TransportOpener = std::function<std::unique_ptr<HidTransport>(const std::string& devicePath)>;

std::unique_ptr<HidTransport> OpenHidraw(const std::string& devicePath);

class HidrawNode
{
public:
    std::string     devicePath;
    unsigned short  vendorId  = 0;
    unsigned short  productId = 0;
    std::string     physicalPath;
    ByteVector      reportDescriptor;

    std::string                     UsbDevice() const;
    bool                            DescriptorContains(const ByteVector& bytes) const;
    static std::vector<HidrawNode>  Find(unsigned short wantedVendorId, unsigned short wantedProductId);
};

class HidrawHandle : public HidTransport
{
public:
    explicit HidrawHandle(const std::string& devicePath);
    HidrawHandle(const HidrawHandle&) = delete;
    HidrawHandle& operator=(const HidrawHandle&) = delete;
    ~HidrawHandle() override;

    const std::string&          Path() const override;
    void                        Write(const ByteVector& report) override;
    std::optional<ByteVector>   Read(std::chrono::milliseconds timeout) override;
    void                        SendFeature(const ByteVector& report) override;
    ByteVector                  GetFeature(std::size_t length) override;
    void                        LockExclusive() override;

private:
    int         descriptor = -1;
    std::string path;
};
