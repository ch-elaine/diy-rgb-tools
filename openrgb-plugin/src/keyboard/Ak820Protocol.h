#pragma once

#include <ctime>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Hidraw.h"

class KeyboardLighting
{
public:
    unsigned char   modeCode   = 0x01;
    unsigned char   red        = 0xFF;
    unsigned char   green      = 0xFF;
    unsigned char   blue       = 0xFF;
    bool            rainbow    = false;
    unsigned char   brightness = 5;
    unsigned char   speed      = 3;
    unsigned char   direction  = 0;

    ByteVector      Packet() const;
};

class Ak820Interfaces
{
public:
    std::string                             controlPath;
    std::string                             displayPath;

    static std::optional<Ak820Interfaces>   Find();
};

class Ak820Protocol
{
public:
    static constexpr std::size_t    ReportSize      = 64;
    static constexpr std::size_t    ScreenChunkSize = 4096;

    explicit Ak820Protocol(HidTransport& controlTransport);

    void                SetLighting(const KeyboardLighting& lighting);
    std::tm             SyncClock();
    void                UploadScreen(HidTransport& displayTransport, const ByteVector& payload, unsigned int slot,
                                     const std::function<void(std::size_t, std::size_t)>& progress);

    static ByteVector   CommandPacket(unsigned char command, unsigned char subcommand, unsigned int value);
    static ByteVector   ClockPacket(const std::tm& moment);

private:
    HidTransport&       control;

    ByteVector          Exchange(const ByteVector& packet);
    void                CheckReply(const ByteVector& reply, unsigned char command) const;
    void                SendCommand(unsigned char command, unsigned char subcommand = 0, unsigned int value = 0);
    void                StartSession(unsigned int flag);
    void                FinishSession();
    void                AbandonSession();
    void                WriteSetting(unsigned char command, const std::vector<std::function<ByteVector()>>& packetProducers);
};
