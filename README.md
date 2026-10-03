# ESP32 SWD Debugger for STM32F103

A custom Serial Wire Debug (SWD) host built on an **ESP32**, designed to debug and flash **STM32F103** (ARM Cortex-M3) microcontrollers. It provides an interactive command-line interface (CLI) directly through the ESP32's serial monitor.

## 🚀 Features

- **Interactive Target Control Menu**: Control the debugger via serial terminal.
- **CPU Control**: Halt, Resume, and Reset the target MCU.
- **Single Stepping**: Step through instructions one by one. Features dynamic hardware breakpoint step-over (temporarily toggles the FPB unit to prevent the PC from locking up).
- **Register Dumping**: View CPU registers (R0-R12, SP, LR, PC, xPSR).
- **Hardware Breakpoints**: Set, clear, and list up to 6 hardware breakpoints using the Cortex-M Flash Patch and Breakpoint (FPB) unit.
- **Firmware Flashing**: Flash an embedded `.bin` image directly to the STM32 over SWD.

## 📁 Firmware Flashing Setup (IMPORTANT)

This project has the ability to flash the STM32 target. To do this, the ESP32 needs the STM32 firmware binary embedded into its own application.

**Before building this project:**
1. Compile your target STM32 code (using STM32CubeIDE, Keil, PlatformIO, etc.) to generate a `.bin` file.
2. Rename or copy your generated file to `stm32f103_blink.bin` (or your preferred name).
3. Place this `.bin` file directly inside the `main/` directory of this ESP32 project.
4. If you used a different name than `stm32f103_blink.bin`, ensure you update the `EMBED_FILES` directive in `main/CMakeLists.txt` to match your filename.

## 🛠️ Building & Flashing the ESP32

This project is built using the Espressif IoT Development Framework (ESP-IDF).

1. Open your terminal and load your ESP-IDF environment (e.g., `get_idf`).
2. Build the project:
   ```bash
   idf.py build
   ```
3. Flash to your ESP32 and open the serial monitor:
   ```bash
   idf.py -p /dev/ttyUSB0 flash monitor
   ```
   *(Change `/dev/ttyUSB0` to your actual ESP32 serial port, e.g., `COM3` on Windows)*

## 🎮 How to Use

Once the ESP32 boots up and connects to the STM32 via SWD, press `m` in the serial monitor to bring up the Target Control Menu:

```text
▼ TARGET CONTROL MENU
┌─────┬──────────────────────────────────────────────────┐
│ KEY │ ACTION                                           │
├─────┼──────────────────────────────────────────────────┤
│  h  │ Halt target CPU                                  │
│  r  │ Resume target CPU                                │
│  s  │ Single step instruction                          │
│  c  │ Check CPU status (Run / Halt & PC)               │
│  d  │ Dump CPU registers (R0-R12, SP, LR, PC, xPSR)    │
│  b  │ Hardware breakpoints (Set, Clear, List)          │
│  t  │ Reset target MCU                                 │
│  f  │ Re-flash firmware image                          │
│  m  │ Show / drop down this menu                       │
└─────┴──────────────────────────────────────────────────┘
```

Type the corresponding key to execute the command. 

## 🔌 Hardware Connections

Make sure to connect the ESP32 to the STM32 target using the following ESP32 GPIO pins:
- **SWDIO**: GPIO 19 (Data line)
- **SWCLK**: GPIO 18 (Clock line)
- **NRST**: GPIO 21 (Reset line)
- **GND**: Common ground between ESP32 and STM32
- **3.3V**: (Optional) if the ESP32 is powering the STM32

