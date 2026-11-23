#include "Hidraw.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace
{
    const std::filesystem::path HidrawClassDirectory = "/sys/class/hidraw";
    constexpr std::size_t       ReadBufferSize       = 4096;

    std::string SystemErrorText()
    {
        return std::strerror(errno);
    }

    std::string ReadText(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    std::string UeventValue(const std::string& uevent, const std::string& key)
    {
        const std::string prefix = key + "=";
        std::size_t       start  = 0;

        while(start < uevent.size())
        {
            const std::size_t end  = uevent.find('\n', start);
            const std::string line = uevent.substr(start, end == std::string::npos ? std::string::npos : end - start);

            if(line.rfind(prefix, 0) == 0)
            {
                return line.substr(prefix.size());
            }

            if(end == std::string::npos)
            {
                break;
            }
            start = end + 1;
        }

        return "";
    }
}

void HidTransport::LockExclusive()
{
}

std::unique_ptr<HidTransport> OpenHidraw(const std::string& devicePath)
{
    return std::make_unique<HidrawHandle>(devicePath);
}

void HidTransport::Drain()
{
    while(Read(std::chrono::milliseconds(0)).has_value())
    {
    }
}

std::string HidrawNode::UsbDevice() const
{
    return physicalPath.substr(0, physicalPath.rfind("/input"));
}

bool HidrawNode::DescriptorContains(const ByteVector& bytes) const
{
    return std::search(reportDescriptor.begin(), reportDescriptor.end(), bytes.begin(), bytes.end()) != reportDescriptor.end();
}

std::vector<HidrawNode> HidrawNode::Find(unsigned short wantedVendorId, unsigned short wantedProductId)
{
    std::vector<HidrawNode> nodes;
    std::error_code         error;

    for(const auto& entry : std::filesystem::directory_iterator(HidrawClassDirectory, error))
    {
        const std::string uevent = ReadText(entry.path() / "device" / "uevent");
        const std::string hidId  = UeventValue(uevent, "HID_ID");
        const std::size_t first  = hidId.find(':');
        const std::size_t second = hidId.find(':', first + 1);

        if(first == std::string::npos || second == std::string::npos)
        {
            continue;
        }

        HidrawNode node;
        node.vendorId  = static_cast<unsigned short>(std::strtoul(hidId.substr(first + 1, second - first - 1).c_str(), nullptr, 16));
        node.productId = static_cast<unsigned short>(std::strtoul(hidId.substr(second + 1).c_str(), nullptr, 16));

        if(node.vendorId != wantedVendorId || node.productId != wantedProductId)
        {
            continue;
        }

        const std::string descriptorText = ReadText(entry.path() / "device" / "report_descriptor");
        node.devicePath       = "/dev/" + entry.path().filename().string();
        node.physicalPath     = UeventValue(uevent, "HID_PHYS");
        node.reportDescriptor = ByteVector(descriptorText.begin(), descriptorText.end());
        nodes.push_back(node);
    }

    std::sort(nodes.begin(), nodes.end(), [](const HidrawNode& left, const HidrawNode& right)
    {
        return left.devicePath.size() != right.devicePath.size() ? left.devicePath.size() < right.devicePath.size()
                                                                 : left.devicePath < right.devicePath;
    });
    return nodes;
}

HidrawHandle::HidrawHandle(const std::string& devicePath)
    : path(devicePath)
{
    descriptor = open(devicePath.c_str(), O_RDWR | O_CLOEXEC);

    if(descriptor < 0)
    {
        throw DeviceError("cannot open " + devicePath + ": " + SystemErrorText());
    }
}

HidrawHandle::~HidrawHandle()
{
    if(descriptor >= 0)
    {
        close(descriptor);
    }
}

const std::string& HidrawHandle::Path() const
{
    return path;
}

void HidrawHandle::Write(const ByteVector& report)
{
    const ssize_t written = write(descriptor, report.data(), report.size());

    if(written != static_cast<ssize_t>(report.size()))
    {
        throw DeviceError("write to " + path + " failed: " + (written < 0 ? SystemErrorText() : std::string("short write")));
    }
}

std::optional<ByteVector> HidrawHandle::Read(std::chrono::milliseconds timeout)
{
    pollfd pollDescriptor{ descriptor, POLLIN, 0 };
    const int ready = poll(&pollDescriptor, 1, static_cast<int>(timeout.count()));

    if(ready < 0)
    {
        throw DeviceError("waiting on " + path + " failed: " + SystemErrorText());
    }

    if(ready == 0)
    {
        return std::nullopt;
    }

    if(pollDescriptor.revents & (POLLERR | POLLHUP | POLLNVAL))
    {
        throw DeviceError(path + " went away");
    }

    ByteVector    buffer(ReadBufferSize);
    const ssize_t received = read(descriptor, buffer.data(), buffer.size());

    if(received < 0)
    {
        throw DeviceError("read from " + path + " failed: " + SystemErrorText());
    }

    buffer.resize(static_cast<std::size_t>(received));
    return buffer;
}

void HidrawHandle::SendFeature(const ByteVector& report)
{
    if(ioctl(descriptor, HIDIOCSFEATURE(report.size()), report.data()) < 0)
    {
        throw DeviceError("feature report to " + path + " failed: " + SystemErrorText());
    }
}

ByteVector HidrawHandle::GetFeature(std::size_t length)
{
    ByteVector report(length, 0);
    const int  received = ioctl(descriptor, HIDIOCGFEATURE(length), report.data());

    if(received < 0)
    {
        throw DeviceError("feature report from " + path + " failed: " + SystemErrorText());
    }

    report.resize(static_cast<std::size_t>(received));
    return report;
}

void HidrawHandle::LockExclusive()
{
    if(flock(descriptor, LOCK_EX) < 0)
    {
        throw DeviceError("cannot lock " + path + ": " + SystemErrorText());
    }
}
