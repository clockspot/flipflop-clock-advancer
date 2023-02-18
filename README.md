# flipflop-clock-advancer

**Do you need to toggle a latching relay on schedule in a low-power application? Look no further!**

This project uses an ESP32, triggered by the alarms from a DS3231 RTC, to toggle the relay and then hibernate for maximum power efficiency. Optionally, the ESP32 can also sync the RTC to NTP via WiFi and adjust it for DST changes (TODO).

It is designed to drive Solari di Udine 120 series flip clocks: Cifra (time) and/or Dator/MD (calendar). These clocks use a pair of SPDT switches to advance the display: when the “start” switch is toggled, the DC motor runs until it toggles the “stop” switch. This project replaces the “start” switch with a latching SPDT relay, so the clocks can be driven independently and on battery power.



Two modes are supported, [Cifra](#cifra-mode) and [Dator](#dator-mode):

## Cifra mode

The Cifra 120’s “start” switch is toggled once a minute (in my case, by a synchronous motor). So, in this mode:

* The relay should be attached in place of the “start” switch:
  * COM to white
  * NC to blue/white, NO to brown/white (or vice versa)
* RTC AL2 trips every minute to set (odd minutes) or unset (even minutes) the relay.
* RTC AL1 trips daily at 02:00:10 to perform RTC correction (NTP/DST).
* Power up the system at the top of any even minute (or, if using DST adjustment, at the time defined by `COLD_START_HOUR` in config – NTP sync will render this unnecessary).
* To set the Cifra to the correct time:
  * Bridge the center pins in the connector to advance the minutes (in two-minute increments).
    * If it stays a minute off, see note about pins A0/A1 below.
  * The hour is on a ratchet and can be advanced manually (easiest done with back case off).
* A Dator can be connected to the Cifra and will run as normal.

## Dator mode

The Dator/MD 120’s “start” switch is located inside the Cifra, and is toggled at 23:00 (to “prepare”) and 00:00 (to “advance”). So, in this mode:

* The relay should be attached to:
  * COM to 5V (from the QT Py)
  * NC to Dator’s “prepare” wire, NO to “advance” wire (or vice versa)
* RTC AL1 trips daily at 22:00 to set the relay (“prepare”) and perform RTC correction (NTP/DST).
  * This is done at 22:00 to accommodate “spring forward” days, when the RTC is set to 23:00.
* RTC AL2 trips daily at 00:00 to unset the relay (“advance”).
* Power up the system at the time defined by `COLD_START_HOUR` in config (NTP sync will render this unnecessary).
* To set the Dator to the correct date, remove the back case and:
  * Date: Spin the brass knob clockwise. The weekday will advance too.
  * Weekday: Turn the small ratchet wheel at bottom counterclockwise (you can use the ratchet to advance it, but if the clock is powered, the date will advance too).
  * Month: Pull on the small gray gear at top to unlock the white gear. Turn that gear counterclockwise to advance the months, while watching the tab on the white gear between the month and date displays: this tracks the leap year cycle. When you reach January while the tab is in the downward (6 o’clock) position, you are in a leap year (2020, 2024, 2028, 2032) – go forward from there until you reach the correct month in the correct year.

## Hardware

My prototype implementation uses hardware selections from Adafruit: [QT Py ESP32-S2](https://www.adafruit.com/product/5325) with [BFF charger](https://www.adafruit.com/product/5397) and [battery](https://www.adafruit.com/product/1781); [DS3231 RTC](https://www.adafruit.com/product/5188); and [latching SPDT relay](https://www.adafruit.com/product/2923). The QT Py connections are:

* 5V (from USB or BFF) to:
  * Cifra: the red/white wire from the original power supply
  * Dator: the common wire of the relay
* A0 (GPIO 18) to relay SET pin
* A1 (GPIO 17) to relay UNSET pin
  * Note: If Cifra shows an odd minute instead of even, or Dator “advances” at 22:00, swap these connections.
* A2 (GPIO 9) to BFF if applicable (and enable `BATTERY_MONITOR_PIN` in config)
* A3 (GPIO 8) to RTC SQW (this is the interrupt pin the RTC uses to wake up the ESP32)
* 3V (via pin or STEMMA connector) to RTC 3V/VIN and relay 3V/VIN (see TODO)
* I2C: SDA/SCL to RTC SDA/SCL –or– STEMMA connector to RTC
  * If using STEMMA connector, include `#define Wire Wire1` in config file to use the STEMMA I2C interface (SDA1/SCL1) instead of the regular one (see TODO).
* GND (via pin and/or STEMMA connector, to USB or BFF) to RTC GND, relay GND, and:
  * Cifra: the black wire from the original power supply
  * Dator: the black common wire of the “stop” switch

## TODO

* ~~Essential functionality with RTC alarms~~
* Sync RTC to NTP
* Correct for DST via advance/wait
* Send warning when battery is low
* Switch off +3V circuit (e.g. with MOSFET) while ESP32 is sleeping to save more power and force RTC to run off its own battery
* Better way to trigger an RTC reset, rather than at power-on
* RTClib has native support for alternate I2C interface; use this instead of `#define Wire Wire1`