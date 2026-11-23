# diy-rgb-tools

"DIY RGB Tools", an OpenRGB 1.1 plugin for RGB hardware that OpenRGB doesn't fully handle. It drives these
devices itself, so each one gets a working "Save to device" button that stores the lighting on the hardware:

- **ASRock Polychrome board** (`26ce:01a2`). OpenRGB's own driver never sends the commit, so nothing it sets
  survives a restart. Save commits and reads the result back. Direct can't be stored, so it's stored as Static
  with each zone's first colour.
- **Ajazz AK820 Pro** (`0c45:8009`, by cable). Lighting is written only when you press Save to device, because the
  keyboard keeps every write in flash. The plugin's Keyboard tab uploads a GIF, PNG or JPEG to the screen and
  sets the clock.
- **Thermalright AX120 Digital** (`0416:8001`). Shows CPU or GPU temperature or usage on the digits, or rotates
  through them (all four, or just the two temperatures); set in the Cooler tab and saved with each profile. It has
  three digits, so two values can't be shown side by side. The display has no memory, so something has to keep
  sending it frames: the plugin while OpenRGB runs, and the cooler service otherwise.

RAM keeps OpenRGB's own driver and its own Save to device button (enable it in OpenRGB's ENE SMBus settings). The
Logitech G203 mouse uses OpenRGB's own "Logitech HID++ 2.0" driver. Its firmware draws a stored static colour only
on a cold start, so after a PC reboot it stays dark until OpenRGB sets it; animated effects survive a reboot.

## Cooler service

The Cooler tab's "Install cooler service" button asks for your password once (through polkit) and installs
`diy-rgb-tools-cooler.service`, a small program carried inside the plugin. It starts in early boot (with
`sysinit.target`, a second or two after the bootloader hands over) and drives the display whenever OpenRGB is
closed; while OpenRGB runs, the plugin drives it and the service waits.
Save to device on the cooler rewrites `/etc/diy-rgb-tools/cooler.json`, which the service reloads. After updating
the plugin, the tab offers to update the service too, and "Remove cooler service" takes it all out again.

Installed files: `/usr/local/lib/diy-rgb-tools/diy-rgb-tools-cooler`, `/etc/diy-rgb-tools/cooler.json` (owned by
you) and `/etc/systemd/system/diy-rgb-tools-cooler.service`.

## Building

`openrgb-plugin/build.sh install` fetches the OpenRGB 1.1 plugin headers, builds the service program and the
plugin (with the program inside it), and copies the plugin to `~/.config/OpenRGB/plugins`. Restart OpenRGB
afterwards. It needs Qt 6 (`qmake6`), a C++17 compiler and `curl`.

The plugin leaves the board alone while OpenRGB's own "ASRock Polychrome USB" driver is active. Its Devices tab has
a button that turns that driver off; restart OpenRGB afterwards and save your profiles again once.

Don't run TRCC alongside the plugin or the service: it takes the AX120's USB device away from them.

## Tested on

- ASRock B650M Pro RS, Polychrome USB controller (`26ce:01a2`)
- RAM with ENE RGB controllers (`AUDA0-E6K5-0101`)
- Ajazz AK820 Pro, wired, firmware 1.14 (`0c45:8009`)
- Thermalright AX120 Digital display (`0416:8001`)
