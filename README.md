# XeWe Led OS

# Idea
I noticed that in many ESP/Arduino projects I write the same code again and again.  
This project aims to boil it down to one reusable base of tools that make ESP and Arduino development flow easier.  
Of course what is basic functionality is very subjective, and some might call this project a bloatware.

# Core Features
- Modular design:
  - Allows easy custom code integration on top of the existing functionality
- SerialPort Module
  - Easy serial read/write functionality
- NVS Module
  - Easy interaction with ESP32 non-volatile-storage
- Command Parser
  - Allows to parse text from serial into various software behaviours
- System Module
  - Holds major system-related CLI commands

# CLI Interface
This is a handy way to have a high level control over the LEDs.
You can send multiple commands to configure the state of LEDs.
You can also use another devices to automatically send the commands via serial port.

Commands must follow the structure: $<cmd_group> <cmd_name> <<param_0> <param_1> ... <param_n>>   
Parameters have the range 0-255: $led set_brightness <0-255>   
Parameters must be separated with a space: $led set_rgb <0-255> <0-255> <0-255>   

- To see all commands available type $help
- To see system commands available type $system help
- To see wifi commands available type $wifi help
- To see led commands available type $led help

# Quickstart
tbw
