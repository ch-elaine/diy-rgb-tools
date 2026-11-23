TEMPLATE  = app
CONFIG   += console c++17 silent
CONFIG   -= qt app_bundle
TARGET    = diy-rgb-tools-cooler
DESTDIR   = $$PWD/build/service

INCLUDEPATH +=                              \
    src/common                              \
    src/cooler                              \
    src/service                             \
    OpenRGB                                 \
    OpenRGB/RGBController                   \
    OpenRGB/dependencies/json               \

SOURCES +=                                  \
    src/service/main.cpp                    \
    src/service/CoolerServiceProgram.cpp    \
    src/service/CoolerServiceDaemon.cpp     \
    src/service/CoolerServiceInstaller.cpp  \
    src/common/Hidraw.cpp                   \
    src/cooler/Ax120DisplayLock.cpp         \
    src/cooler/Ax120Frame.cpp               \
    src/cooler/Ax120HidDevice.cpp           \
    src/cooler/Ax120SegmentLayout.cpp       \
    src/cooler/Ax120Settings.cpp            \
    src/cooler/CoolerServiceFiles.cpp       \
    src/cooler/HardwareSensors.cpp          \
