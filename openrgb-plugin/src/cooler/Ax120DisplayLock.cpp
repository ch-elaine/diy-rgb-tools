#include "Ax120DisplayLock.h"

#include <cstddef>
#include <cstring>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace
{
    constexpr std::string_view LockName = "diy-rgb-tools.ax120-display";
}

Ax120DisplayLock::~Ax120DisplayLock()
{
    Release();
}

bool Ax120DisplayLock::TryAcquire()
{
    if(socketDescriptor >= 0)
    {
        return true;
    }

    const int candidate = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);

    if(candidate < 0)
    {
        return false;
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path + 1, LockName.data(), LockName.size());
    const socklen_t addressLength = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + 1 + LockName.size());

    if(bind(candidate, reinterpret_cast<const sockaddr*>(&address), addressLength) != 0)
    {
        close(candidate);
        return false;
    }

    socketDescriptor = candidate;
    return true;
}

void Ax120DisplayLock::Release()
{
    if(socketDescriptor >= 0)
    {
        close(socketDescriptor);
        socketDescriptor = -1;
    }
}
