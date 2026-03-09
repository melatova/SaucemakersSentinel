![Saucier’s Sentinel Logo](saucemakersSentinel/Sentinel_IoT_Device/images/logo.png)

[![Hackster Project](https://img.shields.io/badge/Hackster-Project-blue)](https://www.hackster.io/melatova/saucier-s-sentinel-hot-process-timer-f0ca6f)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Particle%20Photon%202-lightgrey)]()
[![Made With](https://img.shields.io/badge/Made%20With-Care%20%26%20Cayenne-red)]()

-------------------------------------------
--        Saucier’s Sentinel README      --
-------------------------------------------
          (^ . ^)  welcome!


Saucier’s Sentinel — A Hot Process Timer


A chef‑friendly IoT device that monitors a 10‑minute hot‑process cycle for sauce making.  
It reads a K‑type thermocouple, calculates the local boiling point from air pressure,  
and alerts the chef with a playful OLED UI, LED blinks, Philips Hue bulbs, and Wemo smart plugs.

* * * * * * * * * * * * * * * *


Features

  * Precise temperature monitoring** using a MAX6675 K‑type thermocouple  
  * Local boiling‑point calculation** using real‑time pressure from a BME280  
  * Chef‑friendly OLED UI** with animations, progress bar, and alert screens  
  * Smart kitchen integration**  
  * Philips Hue bulbs (blue = too cold, red = too hot)  
  * Wemo smart plugs (radio alert + stove cutoff)  
  * Single‑button interface**  
  * Short press -> start  
  * Long press -> cancel  
  * Stable state machine** for stable, predictable behavior  
  * Custom 3D‑printed enclosure** with slanted front, big easy-to-hit button and OLED window  

······························

System Architecture

loop()
├── readSensors()
├── updateButtons()
├── runStateMachine()
├── updateAlerts()
├── updateLED()
└── updateDisplay()


······························

Hardware

  * Particle Photon 2**
  * MAX6675 thermocouple amplifier**
  * K‑type thermocouple probe**
  * BME280 pressure + humidity sensor**
  * 128×64 SSD1306 OLED (I²C)**
  * Wemo smart plugs**
  * Philips Hue bulbs**
  * Custom 3D‑printed enclosure**

······························

Pinout

| Component         | Pin |
|-------------------|-----|
| Button            | D6  |  
| LED               | D7  |
| Thermocouple CS   | D5  |
| Thermocouple SCK  | D4  |
| Thermocouple MISO | D3  |
| OLED SDA          | D0  |
| OLED SCL          | D1  |
| BME280 SDA        | D0  |
| BME280 SCL        | D1  |


······························

Local Boiling Point Calculation  
*(Clausius‑Clapeyron equation)*

```cpp
float calcBoilingPointC(float pressPA) {
    float pressHPA = pressPA / 100.0;
    float L  = 40650.0;
    float R  = 8.314;
    float T0 = 373.15;
    float P0 = 1013.25;

    float invT = (1.0 / T0) - (logf(pressHPA / P0) * R / L);
    return (1.0 / invT) - 273.15;
}

case SCREEN_CYCLE_IN_PROGRESS:
    if (cycleTimer.isTimerReady()) {
        currentScreen = SCREEN_SUCCESS;
        break;
    }
    if (tempF < TEMP_SAFE_LOW)  currentScreen = SCREEN_ALERT_LOW;
    if (tempF > TEMP_SAFE_HIGH) currentScreen = SCREEN_ALERT_HIGH;
    if (wasCancelHeld()) {
        cancelTimer.startTimer(CANCEL_MS);
        currentScreen = SCREEN_CANCELLING;
    }
    break;

······························

State Machine Core

case SCREEN_CYCLE_IN_PROGRESS:
    if (cycleTimer.isTimerReady()) {
        currentScreen = SCREEN_SUCCESS;
        break;
    }
    if (tempF < TEMP_SAFE_LOW)  currentScreen = SCREEN_ALERT_LOW;
    if (tempF > TEMP_SAFE_HIGH) currentScreen = SCREEN_ALERT_HIGH;
    if (wasCancelHeld()) {
        cancelTimer.startTimer(CANCEL_MS);
        currentScreen = SCREEN_CANCELLING;
    }
    break;

······························

OLED UI

Animated steam on loading screen

Big friendly temperature banner

Progress bar during cycle

Shiver animation (too cold)

Flame flicker (too hot)

Spinner animation (cancelling)

Sparkle animation (success)

······························

Enclosure

Designed in Onshape with:

Slanted ergonomic front

OLED window

Button recess

Thermocouple exit port

Internal mounting bosses

Smooth face blends and fillets

······························

Gallery

Enclosure files
(SaucemakersSentinel/Sentinel_IoT_device/enclosure/)

Internal layout
(SaucemakersSentinel/Sentinel_IoT_device/docs/Temp_monitor_device_fritzing_bb.png)
(SaucemakersSentinel/Sentinel_IoT_device/docs/Temp_monitor_device_fritzing_schem.png)

OLED UI photos
(SaucemakersSentinel/Sentinel_IoT_device/images/OLED_display.png)
(SaucemakersSentinel/Sentinel_IoT_device/images/OLED_Temp_Low.MOV)
(SaucemakersSentinel/Sentinel_IoT_device/images/OLED_complete.MOV)

Thermocouple in pot
(SaucemakersSentinel/Sentinel_IoT_device/images/Habanero_sauce.png)

Final assembled device 
(SaucemakersSentinel/Sentinel_IoT_device/images/device_with_active_screen_front.jpg)

······························

Files

/src — firmware

/enclosure — STLs

/images — photos + renders

/docs — diagrams + schematics

······························

Why This Exists
This project was created for a very special emerging hot‑sauce chef
who needed a reliable, friendly, and scientifically accurate way
to monitor the hot‑process cycle.

······························

License
MIT License — enjoy, remix, and cook boldly.

Project write‑up on Hackster.io:
https://www.hackster.io/melatova/saucier-s-sentinel-hot-process-timer-f0ca6f

[![Hackster Project](https://img.shields.io/badge/Hackster-Project-blue)](https://www.hackster.io/melatova/saucier-s-sentinel-hot-process-timer-f0ca6f)

