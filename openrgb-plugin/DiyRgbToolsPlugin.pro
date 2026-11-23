QT       += core gui widgets
TEMPLATE  = lib
CONFIG   += plugin c++17 silent
TARGET    = OpenRGBDiyRgbToolsPlugin

INCLUDEPATH +=                              \
    src/common                              \
    src/cooler                              \
    src/board                               \
    src/keyboard                            \
    src/plugin                              \
    OpenRGB                                 \
    OpenRGB/RGBController                   \
    OpenRGB/dependencies/json               \

HEADERS +=                                  \
    OpenRGB/OpenRGBPluginInterface.h        \
    src/common/Hidraw.h                     \
    src/common/VirtualDevice.h              \
    src/cooler/Ax120DisplayDriver.h         \
    src/cooler/Ax120DisplayLock.h           \
    src/cooler/Ax120Frame.h                 \
    src/cooler/Ax120HidDevice.h             \
    src/cooler/Ax120SegmentLayout.h         \
    src/cooler/Ax120Settings.h              \
    src/cooler/Ax120SettingsWidget.h        \
    src/cooler/CoolerDevice.h               \
    src/cooler/CoolerServiceFiles.h         \
    src/cooler/HardwareSensors.h            \
    src/board/BoardDevice.h                 \
    src/board/PolychromeProtocol.h          \
    src/keyboard/Ak820Device.h              \
    src/keyboard/Ak820Protocol.h            \
    src/keyboard/KeyboardAnimation.h        \
    src/keyboard/KeyboardWidget.h           \
    src/plugin/CoolerServiceManager.h       \
    src/plugin/CoolerServiceWidget.h        \
    src/plugin/DevicesWidget.h              \
    src/plugin/DiyRgbToolsPlugin.h          \

SOURCES +=                                  \
    src/common/Hidraw.cpp                   \
    src/common/VirtualDevice.cpp            \
    src/cooler/Ax120DisplayDriver.cpp       \
    src/cooler/Ax120DisplayLock.cpp         \
    src/cooler/Ax120Frame.cpp               \
    src/cooler/Ax120HidDevice.cpp           \
    src/cooler/Ax120SegmentLayout.cpp       \
    src/cooler/Ax120Settings.cpp            \
    src/cooler/Ax120SettingsWidget.cpp      \
    src/cooler/CoolerDevice.cpp             \
    src/cooler/CoolerServiceFiles.cpp       \
    src/cooler/HardwareSensors.cpp          \
    src/board/BoardDevice.cpp               \
    src/board/PolychromeProtocol.cpp        \
    src/keyboard/Ak820Device.cpp            \
    src/keyboard/Ak820Protocol.cpp          \
    src/keyboard/KeyboardAnimation.cpp      \
    src/keyboard/KeyboardWidget.cpp         \
    src/plugin/CoolerServiceManager.cpp     \
    src/plugin/CoolerServiceWidget.cpp      \
    src/plugin/DevicesWidget.cpp            \
    src/plugin/DiyRgbToolsPlugin.cpp        \

RESOURCES +=                                \
    DiyRgbToolsPlugin.qrc                   \

DISTFILES +=                                \
    src/plugin/DiyRgbToolsPlugin.json       \
