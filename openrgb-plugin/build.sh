#!/usr/bin/env bash
set -euo pipefail

plugin_directory="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
openrgb_release="release_1.1"
openrgb_raw_url="https://gitlab.com/CalcProgrammer1/OpenRGB/-/raw/$openrgb_release"
sdk_directory="$plugin_directory/OpenRGB"
build_directory="$plugin_directory/build"
plugin_file="libOpenRGBDiyRgbToolsPlugin.so"
plugins_directory="$HOME/.config/OpenRGB/plugins"
installed_plugin="$plugins_directory/$plugin_file"
replaced_plugin="$plugins_directory/libOpenRGBAx120DigitalPlugin.so"

sdk_headers=(
    OpenRGBPluginInterface.h
    ResourceManagerCallback.h
    LogManager.h
    filesystem.h
    RGBController/RGBControllerInterface.h
    dependencies/json/nlohmann/json.hpp
)

for header in "${sdk_headers[@]}"; do
    if [[ ! -f "$sdk_directory/$header" ]]; then
        mkdir -p "$(dirname "$sdk_directory/$header")"
        curl --fail --silent --show-error --location "$openrgb_raw_url/$header" --output "$sdk_directory/$header"
    fi
done

mkdir -p "$build_directory/service-objects"
cd "$build_directory/service-objects"
qmake6 "$plugin_directory/CoolerService.pro"
make --jobs="$(nproc)"

cd "$build_directory"
qmake6 "$plugin_directory/DiyRgbToolsPlugin.pro"
make --jobs="$(nproc)"

if [[ "${1:-}" == "install" ]]; then
    install -D --mode=644 "$build_directory/$plugin_file" "$installed_plugin"
    rm -f "$replaced_plugin"
    echo "Installed $installed_plugin; restart OpenRGB to load it."
fi
