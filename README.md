# flipflop-clock-advancer

**Do you need to toggle a latching relay on schedule in a low-power application? Look no further!**

This project uses an ESP32, triggered by the alarms from a DS3231 RTC, to toggle the relay and then hibernate for maximum power efficiency. Optionally, the ESP32 can also sync the RTC to NTP via WiFi and adjust it for DST changes (TODO).

It is designed to drive Solari Cifra/Dator 120 flip clocks, which use a pair of SPDT switches to control power to the DC drive motor: when Switch A is toggled, the motor runs until it toggles Switch B, so that the display advances one step at a time. Normally, Switch A is toggled externally (in my case, a synchronous motor triggers the Cifra, and the Cifra triggers the Dator); this project replaces Switch A with a latching SPDT relay, so the clocks can be driven independently and on battery power.

My prototype implementation uses hardware selections from Adafruit: [QT Py ESP32-S2](https://www.adafruit.com/product/5325) with [charger](https://www.adafruit.com/product/5397) and [battery](https://www.adafruit.com/product/1781); [DS3231 RTC](https://www.adafruit.com/product/5188); and [latching SPDT relay](https://www.adafruit.com/product/2923).

Two modes are supported:

* **Cifra**
  * AL1 trips daily at 02:00:10 to sync to NTP/adjust for DST
  * AL2 trips every minute at :00 to set/unset relay (per odd/even RTC minute)

* **Dator**
  * AL1 trips daily at 22:00:00 to set relay (Dator prepare) and sync to NTP/adjust for DST
  * AL2 trips daily at 00:00:00 to unset relay (Dator advance)

To set the RTC (before/without NTP sync), manually power up/reset the ESP32 at 00:00:00 (or, if driving a Cifra and unconcerned about DST changes, at the top of any even minute).

## TODO

* Essential functionality with RTC alarms
* Sync RTC to NTP
* Correct for DST via advance/wait
* Send warning when battery is low