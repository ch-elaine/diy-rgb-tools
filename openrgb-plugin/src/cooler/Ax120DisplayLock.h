#pragma once

class Ax120DisplayLock
{
public:
    Ax120DisplayLock() = default;
    Ax120DisplayLock(const Ax120DisplayLock&) = delete;
    Ax120DisplayLock& operator=(const Ax120DisplayLock&) = delete;
    ~Ax120DisplayLock();

    bool    TryAcquire();
    void    Release();

private:
    int     socketDescriptor = -1;
};
