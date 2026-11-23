#include "Ax120DisplayDriver.h"

#include <algorithm>
#include <array>
#include <chrono>
#include "Ax120Frame.h"
#include "LogManager.h"

namespace
{
    using Clock = std::chrono::steady_clock;

    constexpr std::chrono::seconds      SensorInterval{ 1 };
    constexpr std::chrono::seconds      ReconnectInterval{ 2 };
    constexpr std::chrono::hours        UnsupportedModelRetryInterval{ 1 };
    constexpr std::chrono::milliseconds MinimumFrameGap{ 30 };
}

Ax120DisplayDriver::Ax120DisplayDriver(const Ax120DisplayOptions& initialDisplayOptions, const Ax120Lighting& initialLighting, DriverLogFunction logFunction)
    : displayOptions(initialDisplayOptions),
      lighting(initialLighting),
      log(std::move(logFunction))
{
}

Ax120DisplayDriver::~Ax120DisplayDriver()
{
    Stop();
}

void Ax120DisplayDriver::Start()
{
    if(!workerThread.joinable())
    {
        workerThread = std::thread(&Ax120DisplayDriver::Run, this);
    }
}

void Ax120DisplayDriver::Stop()
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        stopRequested = true;
    }
    stateChanged.notify_all();

    if(workerThread.joinable())
    {
        workerThread.join();
    }
}

void Ax120DisplayDriver::SetLighting(const Ax120Lighting& newLighting)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        lighting       = newLighting;
        frameRequested = true;
    }
    stateChanged.notify_all();
}

void Ax120DisplayDriver::SetDisplayOptions(const Ax120DisplayOptions& newDisplayOptions)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        displayOptions = newDisplayOptions;
        frameRequested = true;
    }
    stateChanged.notify_all();
}

Ax120DriverStatus Ax120DisplayDriver::Status() const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return status;
}

void Ax120DisplayDriver::Run()
{
    Clock::time_point nextSensorRead = Clock::now();
    Clock::time_point nextReconnect  = Clock::now();
    Clock::time_point nextRotation;
    SensorReadings    sensorReadings;

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        nextRotation = Clock::now() + std::chrono::seconds(displayOptions.rotationSeconds);
    }

    while(true)
    {
        Ax120DisplayOptions currentOptions;
        Ax120Lighting       currentLighting;

        {
            std::unique_lock<std::mutex> lock(stateMutex);

            Clock::time_point wakeTime = nextReconnect;

            if(device.IsOpen())
            {
                wakeTime = nextSensorRead;

                if(displayOptions.content == DisplayContent::Rotate)
                {
                    wakeTime = std::min(wakeTime, nextRotation);
                }
            }

            stateChanged.wait_until(lock, wakeTime, [this] { return stopRequested || frameRequested; });

            if(stopRequested)
            {
                break;
            }

            frameRequested  = false;
            currentOptions  = displayOptions;
            currentLighting = lighting;
        }

        const Clock::time_point now = Clock::now();

        if(!device.IsOpen())
        {
            if(now < nextReconnect)
            {
                continue;
            }

            nextReconnect = now + ReconnectInterval;

            if(!Connect())
            {
                if(Status().connectionState == ConnectionState::UnsupportedModel)
                {
                    nextReconnect = now + UnsupportedModelRetryInterval;
                }
                continue;
            }
        }

        if(now >= nextSensorRead)
        {
            sensorReadings = sensors.Read();
            nextSensorRead = now + SensorInterval;
        }

        if(currentOptions.content == DisplayContent::Rotate && now >= nextRotation)
        {
            rotationPhase = (rotationPhase + 1) % Ax120Frame::RotationOrder.size();
            nextRotation  = now + std::chrono::seconds(currentOptions.rotationSeconds);
        }

        const SegmentMask mask = Ax120Frame::Mask(currentOptions, sensorReadings, rotationPhase);

        if(device.SendColors(Ax120Frame::Payload(currentLighting, mask)))
        {
            UpdateStatus(ConnectionState::Connected, sensorReadings);
        }
        else
        {
            log(LL_WARNING, "[AX120] Lost the display at " + device.Path() + ", retrying");
            Disconnect();
            nextReconnect = now + ReconnectInterval;
            UpdateStatus(ConnectionState::Searching, sensorReadings);
        }

        std::this_thread::sleep_for(MinimumFrameGap);
    }

    Disconnect();
}

bool Ax120DisplayDriver::Connect()
{
    const ConnectionState previousState = Status().connectionState;

    if(!displayLock.TryAcquire())
    {
        return false;
    }

    if(!device.Open())
    {
        displayLock.Release();
        return false;
    }

    const HandshakeResult handshakeResult = device.Handshake();

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        status.devicePath   = device.Path();
        status.productModel = device.ProductModel();
    }

    if(handshakeResult == HandshakeResult::UnsupportedModel)
    {
        if(previousState != ConnectionState::UnsupportedModel)
        {
            log(LL_ERROR, "[AX120] Device at " + device.Path() + " reports model byte " + std::to_string(*device.ProductModel())
                        + ", which is not a 30-LED AX120-style display; not driving it");
        }
        Disconnect();
        UpdateStatus(ConnectionState::UnsupportedModel, {});
        return false;
    }

    const std::string handshakeDescription = handshakeResult == HandshakeResult::SupportedModel
                                           ? "model byte " + std::to_string(*device.ProductModel())
                                           : "no handshake reply, already answered since power-on";
    log(LL_INFO, "[AX120] Connected to " + device.Path() + " (" + handshakeDescription + ")");
    return true;
}

void Ax120DisplayDriver::Disconnect()
{
    device.Close();
    displayLock.Release();
}

void Ax120DisplayDriver::UpdateStatus(ConnectionState connectionState, const SensorReadings& sensorReadings)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    status.connectionState = connectionState;
    status.sensorReadings  = sensorReadings;
}
