#ifndef CONFIG_H
#define CONFIG_H

#define SHOW_SERIAL
#define ENABLE_NEOPIXEL

// #define MODE_CIFRA //toggles every other minute; with NETWORK_SSID: nightly NTP sync and DST correction at 01:59
#define MODE_DATOR //toggles at 22:00 (with NTP sync if NETWORK_SSID defined) and 00:00

// #define COLD_START_HOUR //Retired. On cold boot without NETWORK_SSID, connect via serial and enter 'r' to set the time manually.

#define USE_DS3231_RTC //Comment this out if you want to use the ESP32's built-in, less accurate but power-efficient RTC. (Without NTP sync, this will drift considerably over time, by as much as a few seconds per hour; but you can add an external 32kHz crystal, or in Dator mode, simply reset it when it starts to change too far away from midnight.)

#define Wire Wire1 //Include this if the DS3231 RTC is connected via Adafruit STEMMA QT connector rather than the standard SDA/SCL pins

// #define I2C_POWER_PIN //TODO: If defined, this pin will go high only when I2C communication is needed to read or write to the DS3231 RTC. If you connect the RTC 3V to this pin, the RTC will run off its own button-cell battery and won't drain the main battery.
// #define I2C_POWER_PIN_ON_FOR_USB_POWER //TODO: If using the Adafruit LiIon/LiPoly Charger BFF, this setting will make the above pin go high when connected to USB power (per pin A2), so that both the main and RTC batteries get recharged. (You can also use a nonrechargeable button-cell for the RTC; if you do, you must ensure that the RTC module doesn't attempt to charge the battery: either power it via the I2C_POWER_PIN with this value set to 0, or disable your RTC module's charging circuit.)

//pins expressed in gpio_num_t format
#define RELAY_UNSET_PIN GPIO_NUM_18 //A0
#define RELAY_SET_PIN GPIO_NUM_17 //A1
#define BATTERY_MONITOR_PIN GPIO_NUM_9 //A2, when QT Py is equipped with LiPo BFF
#define WAKEUP_PIN GPIO_NUM_8 //A3 per https://learn.adafruit.com/adafruit-qt-py-esp32-s2/pinouts

//supply these to enable daily NTP sync
#define NETWORK_SSID "YourNetwork"
#define NETWORK_PASS "YourPassword"

#define NTP_HOST "pool.ntp.org"
#define NTP_HOST2 "time.nist.gov"
#define TIME_ZONE "EST5EDT,M3.2.0,M11.1.0" //TZ_US_Eastern
//#define TIME_ZONE "CST6CDT,M3.2.0,M11.1.0" //TZ_US_Central
//Find your time zone string in https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h

#endif //CONFIG_H