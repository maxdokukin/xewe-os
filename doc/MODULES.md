# Module & Command Reference

The XeWe OS functions through independent modules. Below is a detailed breakdown of each module and its available commands, listed in initialization order.

## SerialPort Module
**Internal Infrastructure**

This module handles UART communication. It manages non-blocking I/O, input buffering, and formatting for the CLI. It serves as the primary interface for user interaction and debugging output.

---

## NVS Module
**Internal Infrastructure**

The Non-Volatile Storage (NVS) module is a wrapper for the ESP32's preferences system. It is responsible for saving and loading configuration data (such as WiFi credentials, Button mappings, or Module states) so they persist after a system reboot.

---

## System Module
**Prefix:** `$system`

The System module acts as the kernel of the OS. It manages the lifecycle of other modules, handles system-wide resets, and provides hardware identification information (MAC, UID, Stack usage).

| Command | Description | Sample Usage |
| :--- | :--- | :--- |
| **`status`** | Get the current status of the System module. | `$system status` |
| **`reset`** | Reset the System module logic. | `$system reset` |
| **`restart`** | Soft restart the ESP32. | `$system restart` |
| **`reboot`** | Alias for restart. | `$system reboot` |
| **`info`** | Displays Chip model, revision, and Build info. | `$system info` |
| **`mac`** | Prints the device MAC addresses. | `$system mac` |
| **`uid`** | Generates a unique Device UID from the eFuse base MAC (and SHA256-64). | `$system uid` |
| **`stack`** | Prints the current task stack watermark (in words). | `$system stack` |

---

## CommandParser Module
**Internal Infrastructure**

This is the text processing engine of the OS. It takes raw string input from the Serial Port or Web Interface, tokenizes the arguments, and routes them to the appropriate module callback function.

---

## Pins Module
**Prefix:** `$pins`

The Pins module serves as a Hardware Abstraction Layer (HAL). It allows you to manipulate GPIOs directly without writing new code. It supports Digital I/O, Analog Reading (ADC), PWM generation, and I2C scanning.

| Command | Description | Sample Usage |
| :--- | :--- | :--- |
| **`status`** | Get module status. | `$pins status` |
| **`reset`** | Reset the module. | `$pins reset` |
| **`enable`** | Enable the Pins module. | `$pins enable` |
| **`disable`** | Disable the Pins module. | `$pins disable` |
| **`gpio_read`** | Read digital logic level (0 or 1). Configures pin as INPUT. | `$pins gpio_read <pin>` |
| **`gpio_write`** | Force pin to HIGH (1) or LOW (0). Configures pin as OUTPUT. | `$pins gpio_write <pin> <0\|1>` |
| **`gpio_toggle`** | Inverts the current state (High to Low or Low to High). Forces OUTPUT mode. | `$pins gpio_toggle <pin>` |
| **`gpio_mode`** | Set IO mode. Options: `in`, `out`, `in_pullup`, `in_pulldown`. | `$pins gpio_mode <pin> <mode>` |
| **`adc_read`** | Read analog voltage. Returns raw integer (0-4095). | `$pins adc_read <pin>` |
| **`pwm_setup`** | Attach PWM timer. Freq: 1Hz-40MHz. Bits: 1-16. | `$pins pwm_setup <pin> <hz> <bits>` |
| **`pwm_write`** | Set PWM duty cycle. Max value is (2^bits) - 1. | `$pins pwm_write <pin> <duty>` |
| **`pwm_stop`** | Stop PWM (duty 0). Add `1` as 2nd arg to detach hardware. | `$pins pwm_stop <pin> [detach]` |
| **`i2c_scan`** | Init I2C on pins and scan for devices (0x01 - 0x77). | `$pins i2c_scan <sda> <scl>` |

---

## Buttons Module
**Prefix:** `$buttons`

The Buttons module handles physical input. It provides software debouncing and allows you to bind any system command (or sequence of commands) to a physical button event (Press, Release, or Change).

| Command | Description | Sample Usage |
| :--- | :--- | :--- |
| **`status`** | Get module status. | `$buttons status` |
| **`reset`** | Reset the module. | `$buttons reset` |
| **`enable`** | Enable the Buttons module. | `$buttons enable` |
| **`disable`** | Disable the Buttons module. | `$buttons disable` |
| **`add`** | Add a button mapping.<br>**Args:** `<pin> "<cmd>" [mode] [trigger] [debounce]`<br>**Modes:** `pullup`, `pulldown`<br>**Triggers:** `on_press`, `on_release`, `on_change` | `$buttons add 9 "$system reboot" pullup on_press 50` |
| **`remove`** | Remove a button mapping by pin number. | `$buttons remove 9` |

---

## Wifi Module
**Prefix:** `$wifi`

The Wifi module manages the ESP32's network connection. It handles connecting to credentials stored in NVS and scanning for available networks.

| Command | Description | Sample Usage |
| :--- | :--- | :--- |
| **`status`** | Get connection status and IP. | `$wifi status` |
| **`reset`** | Reset the module. | `$wifi reset` |
| **`enable`** | Enable WiFi (starts radio). | `$wifi enable` |
| **`disable`** | Disable WiFi (saves power). | `$wifi disable` |
| **`connect`** | Attempt to connect/reconnect using stored credentials. | `$wifi connect` |
| **`disconnect`**| Disconnect from current AP. | `$wifi disconnect` |
| **`scan`** | List available WiFi networks (SSID/RSSI). | `$wifi scan` |

---

## Web Interface Module
**Prefix:** `$web_interface`

The Web Interface module spins up an HTTP server that allows other devices on the same network to send CLI commands to the XeWe OS via a web browser or API calls.

| Command | Description | Sample Usage |
| :--- | :--- | :--- |
| **`status`** | Get server status. | `$web_interface status` |
| **`reset`** | Reset the module. | `$web_interface reset` |
| **`enable`** | Start the Web Interface server. | `$web_interface enable` |
| **`disable`** | Stop the Web Interface server. | `$web_interface disable` |