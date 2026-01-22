# XeWe OS

---

<img src="static/media/resources/readme/boot_log.webp" style="max-width:300px;width:100%;height:auto;">


---

## The Idea

I noticed that in many ESP/Arduino projects I write the same code again and again.
This project aims to boil it down to one reusable base of tools that make ESP and Arduino development flow easier.

Of course, what constitutes "basic functionality" is very subjective, and some might call this project bloatware.
**One man's boilerplate is another man's bloatware.**

## Core Features

**Modular Design:** The system allows easy custom code integration on top of existing functionality. Features are independent units that can be configured, persisted to NVS, and controlled via the CLI.

### Core Modules

* **System Module:** Holds major system-related CLI commands and acts as the kernel.
* **SerialPort Module:** Robust serial read/write functionality with non-blocking I/O.
* **NVS Module:** Easy interaction with ESP32 Non-Volatile Storage to save settings across reboots.
* **Command Parser:** Parses text streams into actionable software behaviors.

### Optional Modules

* **Pins:** Direct hardware abstraction for GPIO, ADC, PWM, and I2C.
* **Buttons:** Binds physical buttons to command execution with software debouncing.
* **Wifi:** Manages network connections.
* **Web Interface:** Allows sending commands from devices on the same WiFi network.

To see all modules and commands: [MODULES.md](doc/MODULES.md)

---
## How You Can Benefit
If you are developing/vibe-coding a ESP application, this project is a great starting point. It allows to keep work in independent modules, to make it reusable and maintainable.   

So for example, if you want to control a PWM fan via web browser, you can just implement Fan Module, without worrying about Wifi and pwm.

## Quickstart

### Option 1: The Easy Way (Web Flasher)

Upload a precompiled binary file directly from your browser:

1. Go to **[maxdokukin.com/projects/xewe-os](https://maxdokukin.com/projects/xewe-os)**
2. Scroll down to **Firmware Flasher**.
3. Connect your board and follow the instructions.

### Option 2: Build from Source

To build and upload the code manually using the provided scripts:

```bash
cd build/scripts
./setup_build_enviroment.sh

# Syntax: ./build.sh -t <target_chip> -p <port>
./build.sh -t c3 -p /dev/cu.usbmodem11143201

```

---

## CLI Interface

This framework provides a handy way to have high-level control over the device at runtime. You can use the Serial Monitor or other devices (via Web/WiFi) to automatically send commands.

To see all modules and commands: [MODULES.md](doc/MODULES.md)

### Syntax

Commands must follow the structure:
`$<cmd_group> <cmd_name> <param_0> <param_1> ...`

* **Prefix:** All commands start with `$`.
* **Spacing:** Parameters must be separated by a space.
* **Help:** To see all commands available, type `$help`. To see system commands, type `$system`.

### Some examples

```bash
# 1. Get chip model and build info
$system info

# 2. Toggle the built-in LED (usually GPIO 2)
$pins gpio_toggle 2

# 3. Scan for available WiFi networks
$wifi scan

# 4. Bind the "BOOT" button (GPIO 0) to toggle the LED when pressed
$buttons add 0 "$pins gpio_toggle 8" pullup on_press 50
```

---

## Architecture & Development

The system revolves around the `SystemController`, which acts as the manager. It initializes and manages the lifecycle of all `Module` instances.

### Directory Structure

```text
src/
├── SystemController/   # Kernel/Manager
├── Modules/            # Feature implementations
│   ├── Hardware/       # Physical interface (Pins, Buttons)
│   ├── Software/       # Logic (Wifi, WebInterface, NVS)
│   └── Module/         # Base class definition
├── XeWeStringUtils.h   # Zero-allocation string helpers
└── Debug.h             # Conditional debug macros

```
For more see [PROJECT_STRUCTURE.md](doc/PROJECT_STRUCTURE.md)
To add your own module, see [ADDING_A_MODULE.md](doc/ADDING_A_MODULE.md)

### Debugging

The project uses `Debug.h` to control verbosity. You can enable debug prints per-module by changing definitions to `1` in `src/Debug.h`:

```cpp
#define DEBUG_SystemController  0
#define DEBUG_Pins              1  // <--- Enables debug logs for Pins
#define DEBUG_Wifi              0

```

---

## License

**PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0**

* **Non-Commercial**: You may use this software for personal or educational purposes.
* **No AI**: You may not use this codebase to train AI models.

*Copyright 2026 Maxim Dokukin*