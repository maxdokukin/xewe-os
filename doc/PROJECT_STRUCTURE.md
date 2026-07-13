```text
xewe-os/                                           # Project root (ESP32 “xewe-os” firmware + tooling)
│
├── build/                                         # Build tooling + build artifacts
│   ├── releases/                                  
│   ├── builds/                                    
│   │   ├── .version_state                         # Tracks version/build state used by scripts (bump/last build info)
│   │   ├── latest/                                # Pointer to most recent build output
│   │   ├── cache/                                 # Compiler/toolchain cache
│   │   └── DATETIME-VERSION-ESP32-CHIP-xewe-os/   # One build “snapshot” (logs, binaries, merged images, copied src)
│   │
│   └── scripts/                                   # Build scripts, organized by platform
│       ├── mac/                                   # macOS pipeline (bash)
│       │   ├── setup_build_environment.sh         # One-time env setup (Homebrew: arduino-cli, ESP32 core, venv, libs)
│       │   ├── build.sh                           # Orchestrates compile + optional upload + serial monitor
│       │   ├── compile.sh                         # Performs compilation of the src
│       │   ├── upload.sh                          # Flashes the merged image via esptool
│       │   ├── listen_serial.sh                   # Monitor serial port
│       │   └── release.sh                         # Builds/pushes for multiple targets, bumps version_state
│       ├── linux/                                 # Linux: apt-based setup + thin stubs forwarding to ../mac
│       │   ├── setup_build_environment.sh         # One-time env setup (apt: arduino-cli, ESP32 core, venv, libs)
│       │   └── build.sh, compile.sh, upload.sh, listen_serial.sh, release.sh   # Stubs: cd ../mac && exec the shared script
│       ├── windows/                               # Windows pipeline (PowerShell)
│       │   ├── setup_build_environment.ps1        # One-time env setup (winget: arduino-cli, Python, Git; ESP32 core, venv, libs)
│       │   ├── build.ps1                          # Orchestrates compile + optional upload + serial monitor
│       │   ├── compile.ps1                        # Performs compilation of the src
│       │   ├── upload.ps1                         # Flashes the merged image via esptool
│       │   └── listen_serial.ps1                  # Monitor serial port
│       └── other/                                 # Misc helpers (e.g. resilient ESP32 core installer)
│
├── doc/                                           
│   ├── ADDING_A_MODULE.md                         # How to create/register a new module in this architecture
│   ├── BUILD_TOOLS.md                             # Notes on build tooling/scripts and expected environment
│   ├── CONTRIBUTING.md                            # Contribution rules (style, workflow, PR expectations)
│   ├── PROJECT_STRUCTURE.md                       # High-level layout and responsibilities of folders/modules
│   └── license_header.txt                         # Standard license header text to paste into new files
│
├── src/                                          
│   ├── Debug.h                                    # Debug/logging macros, flags, and helpers
│   ├── XeWeStringUtils.h                          # Shared string utilities
│   ├── build_info.h                               # Build metadata
│   ├── Modules/                                   # Modular feature units
│   │   ├── Hardware/                              # Modules that touch GPIO/peripherals directly
│   │   │   ├── Buttons/                           # Button input handling (read/debounce/events)
│   │   │   │   ├── Buttons.cpp                    
│   │   │   │   └── Buttons.h                      
│   │   │   └── Pins/                              # Pin read write, PWM, and ADC
│   │   │       ├── Pins.cpp                       
│   │   │       └── Pins.h                         
│   │   ├── Module/                                # Base module framework
│   │   │   ├── Module.cpp                         
│   │   │   └── Module.h                           
│   │   └── Software/                              # Modules providing higher-level services
│   │       ├── CommandParser/                     # Parses commands (CLI/serial/web commands → actions)
│   │       │   ├── CommandParser.cpp              
│   │       │   └── CommandParser.h                
│   │       ├── Nvs/                               # Non-volatile storage wrapper (ESP32 NVS key/value)
│   │       │   ├── Nvs.cpp                        
│   │       │   └── Nvs.h                          
│   │       ├── SerialPort/                        # Serial I/O abstraction with handy methods
│   │       │   ├── SerialPort.cpp                 
│   │       │   └── SerialPort.h                   
│   │       ├── System/                            # System services (state, timing, restart, metrics, etc.)
│   │       │   ├── System.cpp                     
│   │       │   └── System.h                       
│   │       ├── WebInterface/                      # HTTP/web UI endpoints + handlers
│   │       │   ├── WebInterface.cpp               
│   │       │   └── WebInterface.h                 
│   │       └── Wifi/                              # Wi-Fi connectivity management (join/AP, reconnect, status)
│   │           ├── Wifi.cpp                       
│   │           └── Wifi.h                         
│   └── SystemController/                          # Application orchestrator
│       ├── SystemController.cpp                   
│       └── SystemController.h                     
│
├── src_templates/                                 # Starter templates for generating new modules/files
│   ├── ModuleTemplate.cpp                         # Example implementation skeleton
│   └── ModuleTemplate.h                           # Example header skeleton
│
└── static/                                        # Static assets (images/files served by web UI or used in docs)
```