#ifndef CONFIG_H
#define CONFIG_H

#define SHOW_SERIAL
#define ENABLE_NEOPIXEL

#define MODE_CIFRA //toggles every other minute (with correction at 02:00)
//#define MODE_DATOR //toggles at 22:00 (with correction) and 00:00

#define COLD_START_HOUR 18 //When first powered up, the RTC will be set to the top of the hour specified here. If driving a Dator, or Cifra with DST correction, you'll need to perform the initial power-up at this time to set the RTC correctly, so choose a time that's convenient for you. (Not as relevant if driving a Cifra without DST correction, or if NTP sync is enabled.)

#define Wire Wire1 //Include this if the ESP32/RTC are connected via Adafruit STEMMA QT connector rather than the standard SDA/SCL pins

//pins expressed in gpio_num_t format
#define RELAY_UNSET_PIN GPIO_NUM_18 //A0
#define RELAY_SET_PIN GPIO_NUM_17 //A1
#define BATTERY_MONITOR_PIN GPIO_NUM_9 //A2, when QT Py is equipped with LiPo BFF
#define WAKEUP_PIN GPIO_NUM_8 //A3 per https://learn.adafruit.com/adafruit-qt-py-esp32-s2/pinouts

//#define ENABLE_NTP_SYNC

#endif //CONFIG_H