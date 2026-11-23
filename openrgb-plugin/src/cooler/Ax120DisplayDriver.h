#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include "Ax120DisplayLock.h"
#include "Ax120HidDevice.h"
#include "Ax120SegmentLayout.h"
#include "Ax120Settings.h"
#include "HardwareSensors.h"

enum class ConnectionState
{
    Searching,
    Connected,
    UnsupportedModel,
};

class Ax120DriverStatus
{
public:
    ConnectionState                 connectionState = ConnectionState::Searching;
    std::string                     devicePath;
    std::optional<unsigned char>    productModel;
    SensorReadings                  sensorReadings;
};

using DriverLogFunction = std::function<void(unsigned int logLevel, const std::string& message)>;

class Ax120DisplayDriver
{
public:
    Ax120DisplayDriver(const Ax120DisplayOptions& initialDisplayOptions, const Ax120Lighting& initialLighting, DriverLogFunction logFunction);
    Ax120DisplayDriver(const Ax120DisplayDriver&) = delete;
    Ax120DisplayDriver& operator=(const Ax120DisplayDriver&) = delete;
    ~Ax120DisplayDriver();

    void                Start();
    void                Stop();
    void                SetLighting(const Ax120Lighting& newLighting);
    void                SetDisplayOptions(const Ax120DisplayOptions& newDisplayOptions);
    Ax120DriverStatus   Status() const;

private:
    mutable std::mutex          stateMutex;
    std::condition_variable     stateChanged;
    Ax120DisplayOptions         displayOptions;
    Ax120Lighting               lighting;
    Ax120DriverStatus           status;
    bool                        frameRequested = true;
    bool                        stopRequested  = false;
    DriverLogFunction           log;
    std::thread                 workerThread;

    Ax120DisplayLock            displayLock;
    Ax120HidDevice              device;
    HardwareSensors             sensors;
    std::size_t                 rotationPhase  = 0;

    void                        Run();
    bool                        Connect();
    void                        Disconnect();
    void                        UpdateStatus(ConnectionState connectionState, const SensorReadings& sensorReadings);

};
