# Blinky

Minimal ESP32-C3 firmware for hairtag. Every second it:

- Toggles the onboard LED (GPIO10, per schematic)
- Prints `alive! + {idx}` over serial

## Usage

```bash
pio run -t upload
pio device monitor
```
