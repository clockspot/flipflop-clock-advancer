* Network: Send warning when battery is low
* Network: log activity somewhere?
* Power I2C via GPIO pin to force DS3231 to run off its own battery
* RTClib has native support for alternate I2C interface; use this instead of `#define Wire Wire1`
* ESP32 RTC can use an external 32kHz crystal; does QT Py expose the necessary pins?