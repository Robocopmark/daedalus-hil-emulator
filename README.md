# 🤖 RobStride QDD to Linux HIL Bridge
**A bare-metal C++ & Python teleoperation bridge for Seeed Studio RobStride/Cybergear actuators, bypassing proprietary USB-to-CAN firmware on embedded Linux (Jetson Orin).**

## 📖 Overview
Integrating high-torque QDD (Quasi-Direct Drive) actuators into embedded Linux Hardware-in-the-Loop (HIL) systems frequently hits two major roadblocks:
1. Stripped embedded kernels (like NVIDIA Jetpack) lacking native `slcan` SocketCAN networking modules.
2. Proprietary USB-to-CAN adapters that reject standard ASCII CAN commands in favor of closed-source binary protocols.

This repository provides a reverse-engineered, bare-metal solution. By treating the Seeed Studio adapter as a raw memory-mapped POSIX interface and disabling OS-level text mutations, we achieve deterministic 100Hz+ control over the motor without relying on vendor GUIs or kernel-level CAN bridges.

## ✨ Features
* **Bare-Metal C++ Core:** A highly optimized POSIX driver that writes raw hexadecimal byte arrays directly to `/dev/ttyUSB0`.
* **OS-Level Corruption Fix:** Implements `cfmakeraw()` to explicitly bypass Linux `OPOST` text formatting, preventing the OS from injecting carriage returns (`0x0D`) that corrupt binary payloads.
* **DualSense Teleoperation:** A Python tool using `pygame` and `pyserial` to dynamically map PlayStation 5 controller inputs into the proprietary 16-bit binary payload for real-time teleoperation.
* **Hardware Fault Mocking:** An included Arduino Mega emulator script to test HIL physical relay fault injections safely.
* **Docker Ready:** Fully containerized with `--device` passthrough for reproducible builds.

---

## 🛠️ The Reverse-Engineered Protocol

The GD32 ARM Cortex-M chip inside the official Seeed Studio USB-to-CAN adapter does not parse standard SLCAN (e.g., `T0300007F8000000000...`). It uses a proprietary 17-byte binary sequence wrapped in AT command headers.

**Baud Rate:** `921600`
**Frame Format:** 17 Raw Bytes

| Byte Index | Purpose | Example / Value |
| :--- | :--- | :--- |
| `0-1` | **AT Header** | `0x41, 0x54` (ASCII 'A', 'T') |
| `2-11` | **Command & CAN ID** | `0x90, 0x07, 0xEB...` (Byte 3 `0x07` maps to Motor ID `0x7F`) |
| `12-14` | **Dynamic Payload** | Target Velocity/Position offset |
| `15-16` | **Terminator** | `0x0D, 0x0A` (`\r\n`) |

> **Key Insight - Motor Movement:** The manufacturer encodes zero movement/torque as the 16-bit integer midpoint: **`0x7FFF`** (32767). To move forward, add to this midpoint; to reverse, subtract from it.

### Static Binary Payloads (Motor ID `0x7F`)
* **Wake / Enable:** 
  `{0x41, 0x54, 0x18, 0x07, 0xEB, 0xFC, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x0A}`
* **Safe Stop / Disable:** 
  `{0x41, 0x54, 0x90, 0x07, 0xEB, 0xFC, 0x08, 0x05, 0x70, 0x00, 0x00, 0x07, 0x00, 0x7F, 0xFF, 0x0D, 0x0A}`

---

## 🚀 Quick Start (Jetson / Linux)

### Prerequisites
* NVIDIA Jetson (or any Ubuntu-based Linux host)
* Seeed Studio RobStride 06 QDD Actuator + USB-to-CAN Driver Board
* 24V DC Power Supply (e.g., Mean Well NDR-120-24)
* Docker

### 1. Hardware Setup
Connect the 24V supply to the actuator, wire the CAN_H/CAN_L lines to the USB adapter, and plug the adapter into the host. Verify the connection:
```bash
ls /dev/ttyUSB*
# Should output: /dev/ttyUSB0
