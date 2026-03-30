# CYD Flashing/Serial Debugging Log

## Goal

Flash new ESP-IDF v6 firmware (with WiFi, MQTT, HA integration) to the ESP32-8048S043 CYD board.

## Board Details

- ESP32-8048S043 "CYD" — ESP32-S3, 16MB flash, 8MB octal PSRAM
- Single USB-C port, CH340C (SOP-16) USB-to-serial chip
- Headers: P1 (GND, RXD, TXD, 5V), P2 (GPIO 19, 11, 12, 13), P3 (IO17, IO18 "UART1", IO19, IO20 "USB"), P4 (GPIO 18, 17, 3.3V, GND)
- USB-C to USB-C does not work — board lacks CC resistors, requires USB-A to USB-C cable
- Board was previously flashed successfully (method not recorded)

## Confirmed Facts

- **CH340C TX (pin 2) → ESP32 U0RXD (GPIO44, module pin 36)**: continuity confirmed
- **CH340C RX (pin 3) → ESP32 U0TXD (GPIO43, module pin 37)**: continuity confirmed
- **P1 TXD → ESP32 U0TXD (GPIO43)**: continuity confirmed
- **P1 RXD → ESP32 U0RXD (GPIO44)**: continuity confirmed (same net as CH340)
- **BOOT button works**: holding BOOT + pressing RESET stops the display (board enters download mode)
- **Board runs existing firmware**: AQM display shows up on boot (now boot looping after extended troubleshooting)
- **USB-C to USB-C**: confirmed does not enumerate (CC resistor issue)

## What Was Tried

### Via USB-C (CH340)

| Test | Result |
|------|--------|
| USB-A to USB-C via Dell monitor hub | Enumerates as `/dev/cu.usbserial-14130` |
| `idf.py flash` at 460800, 115200, 9600 | "No serial data received" |
| `screen` at 115200 during normal boot | No output |
| `screen` at 115200 in download mode | No output |
| CH340 loopback (short TX/RX on probes at CH340 pins) | No echo — but ESP32 UART0 was still connected, possible bus contention |
| USB-C to USB-C direct to Mac | Does not enumerate |

### Via CP2102 USB-Serial Dongle on P1

| Test | Result | Notes |
|------|--------|-------|
| First cable: dongle on P1, screen 115200 | No output | **Bad cable** — continuity check from ESP32 TX to dongle RX failed |
| New cable (continuity confirmed end-to-end) | No output | |
| screen at 115200, 9600 during normal boot | No output | |
| screen in download mode | No output | |
| idf.py flash at various baud rates, both TX/RX orientations | Failed | |
| Logic analyzer on dongle TX while flashing | **Sees esptool sync data** — dongle TX works | |
| Logic analyzer on dongle RX while flashing in download mode | **No signal from ESP32** | |
| CP2102 blue RX LED | On solid when connected | Initially thought to indicate data; actually just UART idle-high (3.3V) from ESP32 TX line — confirmed by LED turning off when RX lead disconnected |

### Via P3 UART1 (GPIO17/18)

| Test | Result |
|------|--------|
| Dongle on P3 UART1 pins, screen 115200 | No output |

## Current State

- Board is boot looping (displays initial AQM screen briefly, then resets)
- No serial output observed on UART0 in any mode (normal boot, download mode)
- Esptool data reaches the dongle TX pin but ESP32 does not respond
- CH340 loopback inconclusive (ESP32 was still on the bus)

## Hypotheses

### 1. CH340 loopback needs to be retested in isolation

The earlier CH340 loopback test failed, but the ESP32's UART0 pins were still connected. The ESP32's TX driver (idle high) could have been sinking the signal, preventing the loopback from working. Need to redo with CH340 pins shorted at the chip (pins 2-3) to isolate from the ESP32.

### 2. ESP32-S3 UART0 TX is non-functional

Logic analyzer on dongle RX shows no signal from ESP32 even in download mode. The ROM bootloader should always transmit on UART0 in download mode. If GPIO43 is damaged, the ESP32 can receive but not respond.

### 3. Something is interfering with the UART0 lines

The CH340 and the dongle are both connected to the same UART0 bus when using P1 via USB simultaneously. If the CH340 is holding a line in a state that prevents communication, it could block the dongle. When using the dongle alone (board powered from dongle, no USB-C), this shouldn't apply — but worth verifying.

### 4. Boot loop is corrupting the timing

The board is now boot looping. If the reset cycle is fast enough, esptool may not be catching the sync window. However, the ROM bootloader should still transmit its banner on each reset, which the logic analyzer would see.

## Discarded Early Results (Bad Cable)

Early testing with the serial dongle on P1 showed no output. This was initially attributed to wrong baud rate, wrong console config, or P1 not being UART0. All of these were ruled out — the root cause was a **bad cable** with no continuity from ESP32 TX to dongle RX. All dongle-on-P1 results before the cable replacement should be disregarded.

## Next Steps

1. Redo CH340 loopback test through Dell hub — short CH340C pins 2-3 directly on the chip to isolate from ESP32
2. Test the CP2102 dongle + cable with a known-good ESP32 module (C3 SuperMini) to confirm the serial path works end-to-end
3. If serial path confirmed good and ESP32 still doesn't respond: consider GPIO43 damage or try flashing via USB_SERIAL_JTAG on GPIO19/20 (exposed on P2/P3)
