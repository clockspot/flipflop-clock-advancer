# flipflop-clock-advancer

**Low-power ESP32 + DS3231 solution for toggling a SPDT relay, for driving Solari Cifra/Dator 120.**

The Solari Cifra/Dator 120 clock/calendar use a pair of SPDT switches to control power to the DC drive motor: when Switch A is toggled, the motor is powered until it toggles Switch B, so that the display is advanced exactly one minute/date at a time.

In my case, the Cifra's Switch A is toggled by a 1/2rpm AC synchronous motor, so it requires AC to run; the Dator's is toggled by the Cifra's hour display, so it requires a Cifra.

To enable them to run more independently, this project replaces Switch A with a [latching SPDT relay](https://www.adafruit.com/product/2923) toggled by an [ESP32](https://www.adafruit.com/product/5325), which in turn is awoken by the alarms from a [DS3231 RTC](https://www.adafruit.com/product/5188). The ESP32 also optionally syncs the RTC to NTP via WiFi and adjusts for DST changes, then sleeps for maximum power efficiency, in the hope that the whole setup – even the DC motor – can run for months off battery (e.g. a [LiIon](https://www.adafruit.com/product/1781) and [charger](https://www.adafruit.com/product/5397)).

Two cycle modes are supported:

* **Cifra**
  * AL1 daily at 02:00:10 to sync to NTP/adjust for DST
  * AL2 every minute at :00 to set/unset relay (per odd/even RTC minute)

* **Dator**
  * AL1 daily at 22:00:00 to set relay (Dator prepare) and sync to NTP/adjust for DST
  * AL2 daily at 00:00:00 to unset relay (Dator advance)

To set the RTC (before/without NTP sync), manually power up/reset the ESP32 at 00:00:00 (or, if driving a Cifra and unconcerned about DST changes, at the top of any even minute).

## TODO

* Essential functionality with RTC alarms
* Sync RTC to NTP
* Correct for DST via advance/wait
* Send warning when battery is low