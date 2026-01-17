# Generator Hour Meter

An Arduino-based hour meter for tracking generator runtime. Displays total accumulated hours and current session time on a 16x2 LCD display.

## Features

- **Total Runtime Tracking**: Displays cumulative runtime in hours and minutes
- **Session Tracking**: Shows current session runtime with hours, minutes, and seconds
- **Persistent Storage**: Saves total time to EEPROM every minute to preserve data across power cycles
- **Crystal Calibration**: Adjustable PPM correction to compensate for crystal inaccuracy

## Hardware Requirements

- Arduino Nano
- 1602A LCD display (parallel interface)
- Power supply appropriate for your generator's electrical system

## Wiring

LCD connections to Arduino Nano:
- RS → Pin 12
- E → Pin 11
- D4 → Pin 5
- D5 → Pin 4
- D6 → Pin 3
- D7 → Pin 2

## Configuration

### Crystal Calibration

The `CRYSTAL_PPM_CORRECTION` constant can be adjusted to compensate for your specific Arduino's crystal inaccuracy:

- If your clock **gains** time (runs fast): use a **positive** value
- If your clock **loses** time (runs slow): use a **negative** value

**Example calculation:**
- If clock gains 5 seconds per day: `5 / 86400 * 1000000 = ~58 PPM`
- If clock loses 10 seconds per day: `10 / 86400 * 1000000 = ~116 PPM` (use negative)

Typical range: -200 to +200 PPM

### Setting Initial Hours

To set the total hours to a specific value, uncomment and modify the `setTotalHours()` call in `setup()`:
