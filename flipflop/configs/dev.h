#ifndef CONFIG_H
#define CONFIG_H

#define SHOW_SERIAL

#define Wire Wire1 //If using Adafruit QT Py ESP32, with all the I2C stuff connected to the QT port (Wire1) rather than the pins (Wire) - TODO will this mess up your use of the regular Wire pins for other purposes? TODO try to scrap DS3231 library in favor of only RTClib which supports a custom TwoWire interface

#define DEV_STARTUP_PAUSE
#define ENABLE_NEOPIXEL
#define DEV_STAY_AWAKE

#define RELAY_UNSET_PIN A0
#define RELAY_SET_PIN A1

#define BATTERY_MONITOR_PIN A2 //QT Py with LiPo BFF

#define MODE_CIFRA
//#define MODE_DATOR

//#define ENABLE_NTP_SYNC

#endif //CONFIG_H