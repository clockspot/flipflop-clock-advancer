# flipflop-clock-advancer

![Cifra and Dator 120](https://user-images.githubusercontent.com/9385318/219879893-06b7a8ef-29c7-4472-87ab-24f45dd007af.jpeg)

This project is designed to drive Solari Udine 120 series flip clocks (Cifra and Dator/MD). 

These use a pair of SPDT switches to drive the display: when the “start” switch is toggled, a DC motor runs until it toggles the “stop” switch. Here, we replace the “start” switch with a **latching SPDT relay** toggled by an **ESP32 microcontroller.**

* **Cifra mode:** cycles relay every two minutes (set for odd minutes, unset for even ones), to drive a Cifra or Cifra+Dator pair.
* **Dator mode:** cycles relay once a day (set to prepare, unset to advance), to drive a Dator on its own.[^1]
* **Keeps time** with internal RTC or external DS3231 RTC.
* **NTP sync via Wi-Fi** (optional).
* **Automatic DST correction** (if specified).
* **Deep sleep** for maximum power efficiency, to facilitate running on batteries. [See hardware details below.](#hardware)


## Hardware

![QT Py ESP32, BFF charger, and DS3231](https://user-images.githubusercontent.com/9385318/219879905-9410cee5-5b5d-4968-abd2-000e74f9a8f5.jpeg)

My implementation uses hardware selections from Adafruit: a [QT Py ESP32-S2](https://www.adafruit.com/product/5325) connected to a [latching SPDT relay](https://www.adafruit.com/product/2923) and optional [DS3231 RTC](https://www.adafruit.com/product/5188).

#### QT Py connections

* A0 (GPIO 18) to relay UNSET pin
* A1 (GPIO 17) to relay SET pin
* A2 (GPIO 9) to BFF if applicable (and enable `BATTERY_MONITOR_PIN` in config)
* A3 (GPIO 8) to optional DS3231 SQW (this is the interrupt pin the DS3231 uses to wake up the ESP32)
* 3V (via pin or STEMMA connector) to DS3231 3V/VIN and relay 3V/VIN
* I2C: SDA/SCL to DS3231 SDA/SCL –or– STEMMA connector to DS3231
  * If using STEMMA connector, include `#define Wire Wire1` in config file to use the STEMMA I2C interface (SDA1/SCL1) instead of the regular one
* GND (via pin and/or STEMMA connector, to USB or BFF) to DS3231 GND and relay GND

#### Power connections

You can power these 5V components via USB into the QT Py, or with the [QT Py BFF power supply/charger](https://www.adafruit.com/product/5397) and a [LiPo battery](https://www.adafruit.com/product/1781) – in which case you can connect USB periodically to charge the battery (yellow LED goes out when fully charged), or to provide backup running power while you swap out the battery so the clock doesn’t lose time.

The Cifra/Dator drive motors can share this power (by connecting them to the USB/BFF 5V and GND), but I found they were only truly reliable at a higher voltage. If you use the original 9V power supply (or similar) for the motors, you can power the 5V components off this with a [buck converter](https://www.adafruit.com/product/4739).

#### Cifra / Cifra+Dator connections

* Connect the relay in place of the “start” switch:
  * COM to white 
  * NC to blue/white, NO to brown/white
    * If it falls behind by exactly one minute (odd vs. even mismatch), swap these connections or swap your A0/A1 pins.
* Connect Cifra's low-voltage power wires to the original power supply, the 5V/GND from USB/BFF ([with caveat](#power-connections)), or another DC power source:
  * Red/white to power
  * Black to ground
* If you have both a Cifra and a Dator:
  * Connect three pins in the rear connector as shown below.

![IMG_4328a](https://user-images.githubusercontent.com/9385318/219879913-dc18063b-2470-4dc9-b450-c74edaedcc97.jpeg)
My original Cifra install: battery (left) powering ESP32+BFF (right), connected via I2C and A3 (green) to DS3231 RTC (top, front), and via A0/A1 (blue/brown) to relay board (top, behind). The relay is powered from the DS3231's spare STEMMA port, and the clock motor is powered from the 5V/GND pins on the ESP32 (red/black). The synchronous motor (center), which toggled the original “start” switch, is no longer used.

![IMG_4329a](https://user-images.githubusercontent.com/9385318/219879920-c53a7387-0230-49f4-abdf-3d554bf08889.jpeg)
[The relay module](https://www.adafruit.com/product/2923) (top), with its NC/COM/NO terminals connected in place of the original “start” switch (center).

![IMG_4332a](https://user-images.githubusercontent.com/9385318/219879924-d54c484a-eb96-41bc-807c-217640f5a581.jpeg)
The standard connection between the Cifra (left) and Dator (right), comprising “prepare“ (red) and “advance“ (yellow) wires and a ground (black) wire.

#### Dator-only connections

* Connect the relay as follows:
  * COM to [power](#power-connections)
  * NC to Dator’s “prepare” wire and NO to “advance” wire
    * If the Dator advances 1–3 hours before midnight, swap these connections or swap your A0/A1 pins.
* Connect Dator’s black ground wire to [ground](#power-connections)

## Software

* Duplicate `configs/example.h` and edit to suit.
* Specify your config file in `flipflop.h`.
* Compile and upload the sketch to the ESP32 with the Arduino IDE ([see Adafruit guide](https://learn.adafruit.com/adafruit-qt-py-esp32-s2/arduino-ide-setup)).
* To upload a new version of the sketch, reboot the ESP32; the sketch will wait for at least 20 seconds before sleeping to give you a chance to start the upload.

#### On cold boot

* If using NTP sync, [watch the LED](#led-indications) to confirm successful sync.
* If not using NTP sync, set the RTC manually by connecting via serial and entering 'r' within 20 seconds.
* [Manually set the displays](#manual-setting) to the correct time and date. The ESP32 should keep them up to date from there.

#### LED indications

* On cold boot
  * Teal: Successful boot
* During NTP sync
  * Blue: Wi-Fi connected; attempting NTP sync
  * Red: Wi-Fi connection failed
  * Orange: Wi-Fi connected but NTP sync failed
  * Green: NTP sync successful (cold boot only)
* On relay toggle
  * Red flash: Set
  * Green flash: Unset

## Manual setting

#### Cifra 120

* The hour and minute spindles are ratcheted, so you can carefully physically advance them in a forward direction (numbers advancing). 
  * I find this is most easily done by removing the back case and cupping your hand around the flaps, so that you can push on multiple flaps as a unit to move the spindle.
* Alternatively, bridge the center pins in the connector to advance the minutes (in two-minute increments).
* A Dator can be connected to the Cifra (see photo above) and will run as normal.

#### Dator 120

* Remove the case and find the knobs and ratchets on the right-hand side.
* Date: Spin the brass knob clockwise. The weekday will advance too.
* Weekday: Turn the small ratchet wheel at bottom counterclockwise (you can use the ratchet to advance it, but if the clock is powered, the date will advance too).
* Month: Pull on the small gray gear at top to unlock the white gear. Turn that gear counterclockwise to advance the months, while watching the tab on the white gear between the month and date displays: this tracks the leap year cycle. When you reach January while the tab is in the downward (6 o’clock) position, you are in a leap year (2020, 2024, 2028, 2032) – go forward from there until you reach the correct month in the correct year.

## TODO

* ~~Essential functionality with RTC alarms~~
* ~~Retrieve NTP time~~
* ~~Correct for DST via advance/wait~~
* Send warning when battery is low
* Power I2C via GPIO pin to force DS3231 to run off its own battery
* RTClib has native support for alternate I2C interface; use this instead of `#define Wire Wire1`
* It may be possible to connect an external 32kHz crystal

[^1]: This is similar in concept to the [warning period in striking/chiming clocks](https://mb.nawcc.org/wiki/Encyclopedia-Subjects/Clock-Repair/Purpose-of-strike-and-chime-warning).