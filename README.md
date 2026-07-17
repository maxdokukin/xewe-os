# XeWe OS

XeWe OS is a reusable base framework for ESP and Arduino projects. It provides common system, storage, hardware, and networking modules so developers can build application-specific modules without rewriting the same supporting code.

## Overview

The project organizes core and optional functionality into independent modules managed by a central `SystemController`. Modules can be configured, controlled through a command-line interface, and persisted in ESP32 NVS. It is intended for ESP developers who want a modular starting point for device firmware with optional GPIO, button, WiFi, and web control.

## Features

* Modular architecture with independent core and optional modules
* Runtime control through a text-based CLI
* Persistent settings with ESP32 NVS
* Non-blocking serial I/O
* Hardware modules for GPIO, ADC, PWM, and I2C
* Button bindings with software debouncing
* Optional WiFi and local web interface support

## Installation

### Prerequisites

* Supported hardware:

  * ESP32-C3
  * ESP32-C6
  * ESP32-S3
* Git
* A macOS, Linux, or Windows environment for the provided build scripts
* A compatible browser and USB connection for web flashing

### Setup

1. Flash a prebuilt binary from the web flasher:

   1. Open `https://maxdokukin.com/projects/xewe-os`
   2. Scroll to **Firmware Flasher**
   3. Connect the board and follow the instructions

2. Or build from source.

   Clone the repo:

   ```bash
   git clone https://github.com/maxdokukin/xewe-os
   cd xewe-os
   ```

   Build scripts are organized by platform under `build/scripts/<platform>/`. Run
   them from inside that platform folder. `-c` selects the target chip (`c3`, `c6`,
   or `s3`); pass `-p <port>` to also upload and open the serial monitor (omit it to
   compile only).

   **macOS** — `build/scripts/mac/`

   ```bash
   cd build/scripts/mac
   ./setup_build_environment.sh                    # one-time: installs arduino-cli, ESP32 core, venv, libraries

   ls /dev/cu.*                                    # find the port the ESP is connected to

   ./build.sh -c c3                                # compile only
   ./build.sh -c c3 -p /dev/cu.usbmodem11143201    # compile + upload + serial monitor
   ```

   **Linux** — `build/scripts/linux/`

   ```bash
   cd build/scripts/linux
   ./setup_build_environment.sh                    # one-time (apt-based installs)

   ls /dev/ttyUSB* /dev/ttyACM*                    # find the port

   ./build.sh -c c3
   ./build.sh -c c3 -p /dev/ttyUSB0
   ```

   **Windows** — `build/scripts/windows/` (PowerShell)

   ```powershell
   cd build\scripts\windows
   .\setup_build_environment.ps1                   # one-time (winget installs arduino-cli, Python, Git)

   arduino-cli board list                          # find the COM port (e.g. COM5)

   .\build.ps1 -c c3
   .\build.ps1 -c c3 -p COM5
   ```

## Usage

Use the CLI through the serial monitor or through other interfaces that send commands, such as the web interface.

Command syntax:

`$<cmd_group> <cmd_name> <param_0> <param_1> ...`

* All commands start with `$`
* Parameters are separated by spaces
* Use `$help` to list commands
* Use `$system` to list system commands

Examples:

```bash
# Get chip model and build info
$system info

# Toggle the built-in LED
$pins gpio_toggle 2

# Scan for available WiFi networks
$wifi scan

# Bind the BOOT button to toggle GPIO 8 on press
$buttons add 0 "$pins gpio_toggle 8" pullup on_press 50
```

For the full module and command reference, see `doc/MODULES.md`.

## Configuration

* **Libraries:** Edit `build/libraries/required_libraries.txt` and rerun the setup script to install or update required libraries.

Example `required_libraries.txt`:

```text
https://github.com/FastLED/FastLED.git --branch 3.10.3
https://github.com/maxdokukin/xewe-led-library-espalexa
https://github.com/maxdokukin/xewe-led-library-homespan
https://github.com/maxdokukin/xewe-led-library-websockets
https://github.com/bblanchon/ArduinoJson
```

* **Debug settings:** Enable per-module debug output in `src/Debug.h` by setting the relevant flag to `1`.

```cpp
#define DEBUG_SystemController  0
#define DEBUG_Pins              1
#define DEBUG_Wifi              0
```

## Project Structure

* `src/SystemController/` - system manager and module lifecycle control
* `src/Modules/` - module implementations
* `src/Modules/Hardware/` - hardware-facing modules such as Pins and Buttons
* `src/Modules/Software/` - software modules such as Wifi, WebInterface, and NVS
* `src/Modules/Module/` - base module class
* `src/XeWeStringUtils.h` - zero-allocation string helpers
* `src/Debug.h` - debug macros

Additional documentation:

* `doc/PROJECT_STRUCTURE.md`
* `doc/ADDING_A_MODULE.md`

## Development

The system is centered around `SystemController`, which initializes and manages all `Module` instances.

For local development:

* Build and upload with the platform build script — `build/scripts/mac/build.sh`, `build/scripts/linux/build.sh`, or `build/scripts/windows/build.ps1`
* Manage required libraries through `build/libraries/required_libraries.txt`
* Use `src/Debug.h` to enable module-specific debug logging
* See `doc/ADDING_A_MODULE.md` for custom module integration

## License

PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0
