# Aviateur

<p align="center">
  <a href="https://github.com/OpenIPC/aviateur">
    <img src="assets/logo.svg" width="120" alt="Aviateur logo">
  </a>
</p>

<p align="center">
  <strong>OpenIPC FPV ground station for Linux, Windows, and macOS.</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/github/license/OpenIPC/aviateur" alt="License">
  <img src="https://img.shields.io/github/v/release/OpenIPC/aviateur" alt="Release Status">
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-blue" alt="Platform Support">
</p>

---

Aviateur is a high-performance, low-latency FPV ground station specifically designed for
the [OpenIPC](https://openipc.org/) ecosystem. It allows you to receive and display digital video streams from your
drone with minimal lag, supporting modern codecs and hardware acceleration.

![Interface](tutorials/interface.jpg)

## ✨ Features

- **Ultra-Low Latency**: Optimized for real-time FPV flight.
- **Cross-Platform**: Native support for Linux, Windows, and macOS.
- **Flight Recording**: Capture your flights in MP4 or GIF formats.
- **Snapshots**: Save high-quality JPEG screenshots during flight.
- **Hardware Acceleration**: Utilizes GPU for efficient video decoding and rendering (Vulkan/OpenGL).
- **Audio Support**: Real-time audio streaming from the drone.
- **Telemetry & Stats**: Monitor bitrate and link quality in real-time.

## ⚠️ Important Notes

- **Wi-Fi Adapter**: **RTL8812AU** adapters and the **ALFA AWUS1900 (RTL8814AU, `0bda:8813`)** are supported through the bundled devourer userspace driver.
- **AWUS1900 scope**: The tested RunCam WiFiLink 2 receive configuration is channel 161 at 20 MHz. Keep Adaptive Link disabled for RX-only video testing.
- **Adaptive Link**: Not currently supported on Windows.
- **MAVLink**: Basic MAVLink telemetry support is on the roadmap but not yet fully implemented.

## 🚀 Quick Start

### Windows

1. Download [Zadig](https://zadig.akeo.ie/).
2. Select your adapter in Zadig (*Options* → *List All Devices*).
3. Install the **WinUSB** driver for the `0bda:8813` AWUS1900 interface so libusb can claim it.
4. Run `aviateur.exe`.
   > **Note**: If the application fails to start, install
   the [Microsoft Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).

### Linux

1. Set up udev rules (required for non-root access):
    - Run `sudo install -m 0644 80-aviateur-realtek.rules /etc/udev/rules.d/`.
    - Run `sudo udevadm control --reload-rules && sudo udevadm trigger --subsystem-match=usb`.
    - Unplug and reconnect the adapter.
2. Launch AppImage.
   > **Note**: If rules are not set, you must run the application with `root` privileges.

### macOS

1. Build from source (see [Build Instructions](#-how-to-build)).
2. Launch executable.

## 🛠 How to Build

### Prerequisites

- CMake 3.18+
- C++20 compatible compiler
- Core dependencies are FFmpeg, libusb, and libsodium. Linux additionally uses libpcap, Vulkan, and X11 development files. SDL3 is included as a git submodule; OpenCV is optional.

#### Windows cross-build from Linux (MinGW-w64 + vcpkg)

The same Ubuntu host can produce a 64-bit Windows build without Visual Studio or a Windows build machine. Install the MinGW-w64 cross compiler and bootstrap a Linux-hosted vcpkg:

```bash
sudo apt update
sudo apt install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 \
                  binutils-mingw-w64-x86-64 ninja-build pkg-config

git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
export VCPKG_ROOT="$HOME/vcpkg"
"$VCPKG_ROOT/bootstrap-vcpkg.sh"
"$VCPKG_ROOT/vcpkg" install libusb:x64-mingw-dynamic \
                                 ffmpeg:x64-mingw-dynamic \
                                 libsodium:x64-mingw-dynamic
```

Configure a separate Windows build tree. `PKG_CONFIG_LIBDIR` prevents the cross-build from accidentally linking the host's Linux libusb:

```bash
git submodule update --init --recursive
git -C 3rd/devourer checkout e74d57e96b8e47a6ea6dae6778414e1bf09ddbc2

export VCPKG_ROOT="$HOME/vcpkg"
export PKG_CONFIG_LIBDIR="$VCPKG_ROOT/installed/x64-mingw-dynamic/lib/pkgconfig"

cmake -S . -B build-windows -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$PWD/cmake/toolchains/x86_64-w64-mingw32.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic \
  -DPKG_CONFIG_EXECUTABLE=/usr/bin/pkg-config
cmake --build build-windows --parallel
```

Package the vcpkg and MinGW runtime DLLs beside the executables:

```bash
cp "$VCPKG_ROOT/installed/x64-mingw-dynamic/bin/"*.dll build-windows/bin/
cp "$(x86_64-w64-mingw32-g++ -print-file-name=libstdc++-6.dll)" build-windows/bin/
cp "$(x86_64-w64-mingw32-g++ -print-file-name=libgcc_s_seh-1.dll)" build-windows/bin/
cp "$(x86_64-w64-mingw32-g++ -print-file-name=libwinpthread-1.dll)" build-windows/bin/
```

Copy the complete `build-windows/bin` directory to a Windows x64 desktop. It contains `aviateur.exe`, `WiFiDriverDemo.exe`, assets, and runtime DLLs. Install WinUSB for the AWUS1900 with Zadig before testing. MinGW vcpkg triplets are community-supported rather than part of vcpkg's primary CI matrix, so keep the vcpkg revision fixed after obtaining a successful build.

Verify on Linux that the output is a Windows PE executable rather than a Linux ELF executable:

```bash
file build-windows/bin/aviateur.exe
```

On the Windows desktop, open PowerShell in the copied `bin` directory and test the radio before the full application:

```powershell
$env:DEVOURER_PID = "0x8813"
$env:DEVOURER_CHANNEL = "161"
.\WiFiDriverDemo.exe
```

Stop it with `Ctrl+C`, then launch `.\aviateur.exe`. Select the AWUS1900, channel 161, 20 MHz, and the matching `gs.key`. Do not add a private `gs.key` to a distributable build archive.

#### Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config \
                 libavformat-dev libavcodec-dev libswresample-dev \
                 libswscale-dev libavutil-dev libvulkan-dev libusb-1.0-0-dev \
                 libsodium-dev libpcap-dev xorg-dev
git clone --recursive https://github.com/OpenIPC/aviateur
cd aviateur
git -C 3rd/devourer checkout e74d57e96b8e47a6ea6dae6778414e1bf09ddbc2
cmake -S . -B build-awus1900 -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-awus1900 --parallel
```

If the repository was cloned without `--recursive`, initialize it before configuring:

```bash
git submodule update --init --recursive
git -C 3rd/devourer checkout e74d57e96b8e47a6ea6dae6778414e1bf09ddbc2
```

### AWUS1900 Linux test

Confirm the adapter is present as `0bda:8813` and connected at USB 3 speed (`5000M`):

```bash
lsusb
lsusb -t
```

Test the driver receive path independently on channel 161:

```bash
sudo DEVOURER_PID=0x8813 DEVOURER_CHANNEL=161 \
  ./build-awus1900/bin/WiFiDriverDemo
```

Then start Aviateur:

```bash
cd build-awus1900/bin
./aviateur
```

Select `ALFA AWUS1900 (RTL8814AU) - 0bda:8813`, channel 161, `20 MHz`, and the `gs.key` that matches the air unit. Leave Adaptive Link disabled for receive-only testing. The HUD displays all four RTL8814AU receive chains; RTL8812AU adapters continue to display two.

#### macOS (Homebrew)

```bash
brew install pkgconf libusb ffmpeg libsodium libpcap cmake
git clone --recursive https://github.com/OpenIPC/aviateur
mkdir build && cd build
cmake ..
make
```

## 🔍 Troubleshooting

- **Windows Build**: If CMake fails to find packages despite `VCPKG_ROOT` being set, the pre-installed vcpkg from Visual
  Studio might be overriding it. In `CMakeLists.txt`, you may need to explicitly set `CMAKE_TOOLCHAIN_FILE` to your
  vcpkg path.
- **WSL2**: If you are trying to run Aviateur inside WSL2, you will need to map the USB adapter using `usbipd`.
  See [wsl-map-usb.md](wsl-map-usb.md) for details.
- **AWUS1900 not listed**: Verify `lsusb` reports `0bda:8813`, reinstall the udev rule, and unplug/reconnect the adapter.
- **No video on AWUS1900**: Confirm channel 161 / 20 MHz, keep Adaptive Link disabled, and use the same `gs.key` as the air unit.
- **Latency or dropped USB transfers**: Use a high-quality USB cable and confirm `lsusb -t` reports `5000M` for the adapter.
- **`GLXBadFBConfig` in a virtual machine**: Enable 3D acceleration for the VM or use Mesa software rendering. Run from the binary directory so Aviateur can find its packaged assets: `cd build-awus1900/bin && LIBGL_ALWAYS_SOFTWARE=1 ./aviateur`.

## 🚧 Roadmap

- [ ] Zero-Copy YUV renderer.
- [ ] Integrated Ground-side OSD.
- [ ] Full MAVLink telemetry support.
- [ ] Support for additional Wi-Fi chipsets.

## 📄 License

Aviateur is released under the [GPL-3.0 License](LICENSE).

---
<p align="center">Part of the <a href="https://github.com/OpenIPC">OpenIPC Project</a></p>
