// Low-power ESP32 + DS3231 solution for toggling a SPDT relay, for driving e.g. Solari Cifra 120
// https://github.com/clockspot/flipflop-clock-advancer
// Sketch by Luke McKenzie (luke@theclockspot.com)

#include <Arduino.h>
#include "esp_sleep.h"
#include "time.h" //struct tm, time_t, localtime_r(), getLocalTime() — always needed for DST check and no-DS3231 wakeup timing

#if __has_include("include/config.h")
  #include "include/config.h"
#else
  #error "include/config.h not found — copy include/config-example.h to include/config.h and edit to suit."
#endif

// Forward declarations
void relaySet(bool state);
void ntpSync();

#ifdef NETWORK_SSID
  //see https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/Time/SimpleTime/SimpleTime.ino
  #include <WiFi.h>
  #include "esp_sntp.h"
#endif

//RTC objects
#ifdef USE_DS3231_RTC
  #include <RTClib.h> //RTC_DS3231 and DateTime
  RTC_DS3231 rtc;
  DateTime tod; //stores the rtc.now() snapshot for several functions to use
#endif

#ifdef ENABLE_NEOPIXEL
  #include <Adafruit_NeoPixel.h>
  #define NUMPIXELS 1
  Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
#endif

#ifndef RELAY_PULSE
#define RELAY_PULSE 10
#endif

// Sanity check: without a DS3231 or NTP, SHOW_SERIAL is required for manual time entry on cold boot
#if !defined(USE_DS3231_RTC) && !defined(NETWORK_SSID) && !defined(SHOW_SERIAL)
  #error "Without USE_DS3231_RTC and NETWORK_SSID, SHOW_SERIAL must be defined for manual time entry"
#endif

#ifdef MODE_CIFRA
  // Set during fall-back DST detection; cleared when real time (ESP32 system clock) reaches 02:00
  RTC_DATA_ATTR bool waitForFallBack = false;
#endif

#if defined(SHOW_SERIAL) && !defined(NETWORK_SSID)
  //Used only during cold boot.
  int inputStage = 0; // serial menu state: 0=waiting, 1-6=date/time entry, 99=done
  struct tm manualTime; // accumulates fields entered via serial
  unsigned long coldBootStart = 0; // millis() when cold-boot prompt was shown
#endif

void setup(){

  pinMode(RELAY_UNSET_PIN, OUTPUT);
  pinMode(RELAY_SET_PIN, OUTPUT);
  gpio_hold_dis(RELAY_UNSET_PIN);
  gpio_hold_dis(RELAY_SET_PIN);
  digitalWrite(RELAY_UNSET_PIN,LOW);
  digitalWrite(RELAY_SET_PIN,LOW);

  #ifdef USE_DS3231_RTC
    pinMode(WAKEUP_PIN, INPUT_PULLUP);
    gpio_hold_en(WAKEUP_PIN); //https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html#_CPPv316rtc_gpio_hold_en10gpio_num_t
    esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, 0);
    //TODO save further power by leveraging Deep Sleep Wake Stub?
    //https://randomnerdtutorials.com/esp32-deep-sleep-arduino-ide-wake-up-sources/
  #endif

  #ifdef BATTERY_MONITOR_PIN
    pinMode(BATTERY_MONITOR_PIN, INPUT);
  #endif

  #ifdef ENABLE_NEOPIXEL
    #if defined(NEOPIXEL_POWER)
      // If this board has a power control pin, we must set it to output and high
      // in order to enable the NeoPixels. We put this in an #if defined so it can
      // be reused for other boards without compilation errors
      pinMode(NEOPIXEL_POWER, OUTPUT);
      digitalWrite(NEOPIXEL_POWER, HIGH);
    #endif

    pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
    pixels.setBrightness(20); // not so bright
  #endif

  // Set timezone from config so DST rules apply to mktime/localtime even without NTP
  #ifdef TIME_ZONE
    setenv("TZ", TIME_ZONE, 1);
    tzset();
  #endif

  //TODO only need to do this when I2C is powered and communication needs to happen
  #ifdef USE_DS3231_RTC
    rtc.begin();
  #endif

  //Who disturbs my slumber??
  esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();

  if(wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED) { //Cold start

    #ifdef ENABLE_NEOPIXEL
      pixels.fill(0x00FFFF); //teal to indicate hello world
      pixels.show();
    #endif

    #if defined(SHOW_SERIAL)
      //Waiting until this point to begin Serial ensures
      //Serial messsages will only be sent during cold boot.
      Serial.begin(115200);
      delay(2000); //allow serial monitor to connect
      Serial.println("Hello from flipflop-clock-advancer!");
      #if !defined(NETWORK_SSID)
        Serial.println(F("Enter 'r' to set time manually (20s)."));
        coldBootStart = millis();
      #endif
    #endif

    //Attempt to sync to NTP; without NETWORK_SSID, loop() prompts for manual time entry via serial
    #ifdef NETWORK_SSID
      ntpSync();
      delay(20000); //sketch upload window after NTP
    #endif

    //INT pin on DS3231 should be ready to go: "When INTCN is set to logic 1, then a match between
    //the timekeeping registers and either of the alarm registers activates the INT/SQW pin (if the
    //alarm is enabled). Because the INTCN bit is set to logic 1 when power is first applied, the
    //pin defaults to an interrupt output with alarms disabled."
    //Set alarms (DS3231 only)
    #ifdef USE_DS3231_RTC
      #ifdef MODE_CIFRA
        rtc.setAlarm1(DateTime(2020,1,1,1,59,0), DS3231_A1_Hour);      //01:59:00: DST check and NTP sync
        rtc.setAlarm2(DateTime(2020,1,1,0,0,0), DS3231_A2_PerMinute); //every minute: toggle relay
      #endif
      #ifdef MODE_DATOR
        rtc.setAlarm1(DateTime(2020,1,1,22,0,0), DS3231_A1_Hour); //22:00:00: set relay (warning), NTP sync
        rtc.setAlarm2(DateTime(2020,1,1,0,0,0), DS3231_A2_Hour);  //00:00:00: unset relay (advance)
      #endif
    #endif

    //We do not advance the clock at cold boot; it is assumed to already be set correctly

  } //end cold start
  else { //woke from sleep

    #ifdef USE_DS3231_RTC
      tod = rtc.now(); //take snapshot of current time

      //Check alarm2 first: relay action before DST check and NTP sync
      if(rtc.alarmFired(2)) {
        #ifdef MODE_CIFRA
          //During fall-back wait, check if real time (ESP32 system clock) has reached 02:00
          if(waitForFallBack) {
            struct tm realNow;
            if(getLocalTime(&realNow) && realNow.tm_hour >= 2) waitForFallBack = false;
          }
          if(!waitForFallBack) {
            if(tod.minute()%2) relaySet(1); //odd minute
            else relaySet(0); //even minute
          }
        #endif
        #ifdef MODE_DATOR
          relaySet(0); //midnight advance
        #endif
      } //end alarmFired(2)

      if(rtc.alarmFired(1)) {
        #ifdef MODE_DATOR
          relaySet(1); //22h (or 21h/23h due to DST change we haven't caught yet)
        #endif
        //DST check for Cifra (Alarm1 fires nightly at 01:59)
        //Compare tm_isdst now vs 2 hours from now: +1 = spring forward, -1 = fall back
        #if defined(MODE_CIFRA) && defined(NETWORK_SSID)
          if(!waitForFallBack) {
            time_t now_t = time(NULL);
            time_t later_t = now_t + 7200;
            struct tm tmNow, tmLater;
            localtime_r(&now_t, &tmNow);
            localtime_r(&later_t, &tmLater);
            int dstDelta = tmLater.tm_isdst - tmNow.tm_isdst;
            if(dstDelta > 0) {
              // Spring forward: rapid advance 62 steps (31 unset+set pairs)
              // Delay-first, following the normal advance to :59 just performed by Alarm2
              // Display ends at :01 (odd); DS3231 corrected to ~03:01 by NTP below
              for(int i = 0; i < 31; i++) {
                delay(2500); relaySet(0); //advance to even minute
                delay(2500); relaySet(1); //advance to odd minute
              }
            } else if(dstDelta < 0) {
              // Fall back: pause relay advances until real time reaches 02:00
              waitForFallBack = true;
            }
          }
        #endif
        //NTP sync last, for both CIFRA and DATOR
        ntpSync(); //set time per NTP and account for DST
      } //end alarmFired(1)

    #else //no DS3231 — timer-based wakeup; NETWORK_SSID required
      struct tm timeinfo;
      bool timeOk = getLocalTime(&timeinfo);
      if(timeOk) {

        #ifdef MODE_CIFRA
          // 1. Relay action: check fall-back wait, then toggle based on current minute
          if(waitForFallBack) {
            if(timeinfo.tm_hour >= 2) waitForFallBack = false;
          }
          if(!waitForFallBack) {
            if(timeinfo.tm_min % 2) relaySet(1); //odd minute
            else relaySet(0); //even minute
          }
          // 2. DST check at :59 of every hour (uses ESP32 system clock, no NTP needed)
          if(timeinfo.tm_min == 59 && !waitForFallBack) {
            time_t later_t = time(NULL) + 7200;
            struct tm tmLater;
            localtime_r(&later_t, &tmLater);
            int dstDelta = tmLater.tm_isdst - timeinfo.tm_isdst;
            if(dstDelta > 0) {
              // Spring forward: rapid advance 62 steps (31 unset+set pairs), delay-first
              for(int i = 0; i < 31; i++) {
                delay(2500); relaySet(0); //advance to even minute
                delay(2500); relaySet(1); //advance to odd minute
              }
            } else if(dstDelta < 0) {
              waitForFallBack = true;
            }
          }
          // 3. NTP sync at :55 (always last)
          if(timeinfo.tm_min == 55) {
            ntpSync();
          }
        #endif

        #ifdef MODE_DATOR
          //Toggle relay before NTP sync, based on current (pre-sync) time.
          //23:00–01:59: assume advance time (unset relay, date flips); otherwise: prepare time (set relay, ~2h warning).
          bool inAdvanceWindow = (timeinfo.tm_hour == 23 || timeinfo.tm_hour <= 1);
          if(inAdvanceWindow) relaySet(0); //advance (unset relay, date flip)
          else {
            relaySet(1); //prepare (set relay, ~2h before advance)
            ntpSync();
          }
        #endif
      
      } //end timeOk
      //TODO: if !timeOk, log it via WiFi?

    #endif //end USE_DS3231_RTC

  } //end woke from sleep

} //end setup()

void loop() {
  // Cold start: handle serial time-entry before sleeping
  #if defined(SHOW_SERIAL) && !defined(NETWORK_SSID)
    if(esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED && inputStage < 99) {
      // Timeout at stage 0: sleep if 'r' not entered within 20 seconds
      if(inputStage == 0 && millis() - coldBootStart >= 20000) {
        inputStage = 99;
      } else {
        // Accumulate characters into inputBuffer; process only on newline
        static String inputBuffer;
        while(Serial.available()) {
          char c = Serial.read();
          if(c == '\r') continue; // ignore CR (Windows line endings)
          if(c == '\n') {
            String line = inputBuffer;
            inputBuffer = "";

            if(inputStage == 0) {
              if(line == "r") {
                memset(&manualTime, 0, sizeof(manualTime));
                manualTime.tm_isdst = -1; // let mktime determine DST from TZ rules
                Serial.println(F("Enter year (e.g. 2026):"));
                inputStage = 1;
              }
              // any other input at stage 0 is ignored; timeout handles exit
            } else {
              // Validate: all digits, non-empty
              bool valid = line.length() > 0;
              for(unsigned int i = 0; i < line.length(); i++) {
                if(!isDigit(line[i])) { valid = false; break; }
              }
              int val = valid ? line.toInt() : -1;

              struct { int lo; int hi; const __FlashStringHelper *prompt; } fields[] = {
                {2000, 9999, F("Enter year (e.g. 2026):")},
                {1,   12,   F("Enter month (1-12):")},
                {1,   31,   F("Enter day (1-31):")},
                {0,   23,   F("Enter hour (0-23):")},
                {0,   59,   F("Enter minute (0-59):")},
                {0,   59,   F("Enter second (0-59):")},
              };
              int idx = inputStage - 1; // fields[0] = stage 1 (year)
              if(!valid || val < fields[idx].lo || val > fields[idx].hi) {
                Serial.print(F("Invalid — ")); Serial.println(fields[idx].prompt);
              } else {
                switch(inputStage) {
                  case 1: manualTime.tm_year = val - 1900; break;
                  case 2: manualTime.tm_mon  = val - 1;    break;
                  case 3: manualTime.tm_mday = val;        break;
                  case 4: manualTime.tm_hour = val;        break;
                  case 5: manualTime.tm_min  = val;        break;
                  case 6: manualTime.tm_sec  = val;        break;
                }
                if(inputStage < 6) {
                  inputStage++;
                  Serial.println(fields[inputStage - 1].prompt);
                } else {
                  // All fields collected — commit
                  time_t epoch = mktime(&manualTime); // normalises fields; sets tm_isdst per TZ rules
                  if(epoch == (time_t)-1) {
                    Serial.println(F("Date/time invalid — please start again. Enter year (e.g. 2026):"));
                    memset(&manualTime, 0, sizeof(manualTime));
                    manualTime.tm_isdst = -1;
                    inputStage = 1;
                  } else {
                    struct timeval tv;
                    tv.tv_sec = epoch;
                    tv.tv_usec = 0;
                    settimeofday(&tv, NULL);
                    #ifdef USE_DS3231_RTC
                      struct tm *ti = localtime(&epoch);
                      rtc.adjust(DateTime(ti->tm_year+1900, ti->tm_mon+1, ti->tm_mday,
                                          ti->tm_hour, ti->tm_min, ti->tm_sec));
                    #endif
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d%s",
                      manualTime.tm_year+1900, manualTime.tm_mon+1, manualTime.tm_mday,
                      manualTime.tm_hour, manualTime.tm_min, manualTime.tm_sec,
                      manualTime.tm_isdst > 0 ? " DST" : "");
                    Serial.print(F("Time set: ")); Serial.println(buf);
                    inputStage = 99;
                  }
                }
              }
            }
          } else {
            inputBuffer += c;
          }
        }
      }
      return; // keep looping; falls through to sleep when inputStage reaches 99
    }
  #endif

  //Once setup is done, clean up and go to sleep
  #ifdef USE_DS3231_RTC
    rtc.clearAlarm(1);
    rtc.clearAlarm(2);
  #endif

  digitalWrite(RELAY_UNSET_PIN,LOW);
  digitalWrite(RELAY_SET_PIN,LOW);
  gpio_hold_en(RELAY_UNSET_PIN);
  gpio_hold_en(RELAY_SET_PIN);

  //For timer-based wakeup (no DS3231): calculate sleep duration to next event
  #ifndef USE_DS3231_RTC
    struct tm timeinfo;
    uint64_t sleepSecs = 60; //fallback
    if(getLocalTime(&timeinfo)) {
      #ifdef MODE_CIFRA
        //Sleep until the next minute boundary
        sleepSecs = 60 - timeinfo.tm_sec;
        if(sleepSecs == 0) sleepSecs = 60;
      #endif
      #ifdef MODE_DATOR
        //Advance window (23:00–01:59): sleep until 22:00 (~22h). Otherwise: sleep until 00:00 (~2h).
        bool inAdvanceWindow = (timeinfo.tm_hour == 23 || timeinfo.tm_hour <= 1);
        int targetHour = inAdvanceWindow ? 22 : 0;
        long currentSecs = (long)timeinfo.tm_hour * 3600 + (long)timeinfo.tm_min * 60 + (long)timeinfo.tm_sec;
        long targetSecs = (long)targetHour * 3600;
        long diffSecs = targetSecs - currentSecs;
        if(diffSecs <= 0) diffSecs += 86400L;
        sleepSecs = (uint64_t)diffSecs;
      #endif
    }
    esp_sleep_enable_timer_wakeup(sleepSecs * 1000000ULL);
  #endif

  #ifdef SHOW_SERIAL
    Serial.flush();
  #endif
  esp_deep_sleep_start();
}

void relaySet(bool state) {
  if(state==1) {
    //set relay for Dator prepare at 22h or Cifra odd minute
    #ifdef ENABLE_NEOPIXEL
      pixels.fill(0xFF0000); //red
      pixels.show();
    #endif
    digitalWrite(RELAY_SET_PIN,HIGH);
    delay(RELAY_PULSE);
    digitalWrite(RELAY_SET_PIN,LOW);
  }
  if(state==0) {
    //unset relay for Dator advance at midnight or Cifra even minute
    #ifdef ENABLE_NEOPIXEL
      pixels.fill(0x00FF00); //green
      pixels.show();
    #endif
    digitalWrite(RELAY_UNSET_PIN,HIGH);
    delay(RELAY_PULSE);
    digitalWrite(RELAY_UNSET_PIN,LOW);
  }
}

void ntpSync() {
  #ifdef NETWORK_SSID
    #ifdef SHOW_SERIAL
      //These messages are only sent out if Serial was begun, i.e. during cold boot.
      Serial.print(F("Connecting to WiFi SSID "));
      Serial.print(NETWORK_SSID);
      Serial.println(F("..."));
    #endif
    WiFi.mode(WIFI_STA);
    WiFi.begin(NETWORK_SSID, NETWORK_PASS);

    bool wifiOk = false;
    for(int timeout=0; timeout<15 && !wifiOk; timeout++) {
      delay(1000);
      if(WiFi.status()==WL_CONNECTED) wifiOk = true;
    }

    if(wifiOk) {
      #ifdef ENABLE_NEOPIXEL
        pixels.fill(0x0000FF); //blue - wifi success
        pixels.show();
      #endif
      #ifdef SHOW_SERIAL
        Serial.println(F("WiFi connected."));
        Serial.print(F("Signal strength (RSSI): ")); Serial.print(WiFi.RSSI()); Serial.println(F(" dBm"));
      #endif

      #ifdef SHOW_SERIAL
        Serial.print(F("Syncing to NTP..."));
      #endif
      //Reset sync status before starting so we know a *fresh* NTP packet was received.
      //Without this, the ESP32 hardware RTC (which persists through deep sleep) makes
      //sntp_get_sync_status() appear complete and getLocalTime() return stale time immediately.
      sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
      //configTzTime starts the SNTP process (non-blocking); POSIX TZ string handles DST automatically.
      #ifdef NTP_HOST2
        configTzTime(TIME_ZONE, NTP_HOST, NTP_HOST2);
      #else
        configTzTime(TIME_ZONE, NTP_HOST);
      #endif
      //Poll for sync completion rather than using getLocalTime(), which returns true
      //based on year > 2016 — not on whether a fresh packet was actually received.
      bool ntpOk = false;
      unsigned long ntpWaitStart = millis();
      while((millis() - ntpWaitStart) < 30000) {
        if(sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
          ntpOk = true;
          break;
        }
        delay(100);
      }

      if(ntpOk) {
        #ifdef SHOW_SERIAL
          Serial.println(F(" synced."));
        #endif
        #ifdef ENABLE_NEOPIXEL
          pixels.fill(0x00FF00); //green
          pixels.show();
        #endif
        //Snapshot gettimeofday() so tv_usec gives the exact sub-second offset with no
        //second-boundary race. Re-derive broken-down time from tv.tv_sec (already TZ-adjusted
        //by configTzTime) so the time is consistent with tv_usec.
        struct timeval tv;
        gettimeofday(&tv, NULL);
        struct tm *ti = localtime(&tv.tv_sec);

        #ifdef USE_DS3231_RTC
          //Update DS3231 from NTP (tm_year is years since 1900; tm_mon is 0-based)
          rtc.adjust(DateTime(ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
                              ti->tm_hour, ti->tm_min, ti->tm_sec));
          #ifdef SHOW_SERIAL
            char buf[20];
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
            Serial.print(F("RTC updated: ")); Serial.println(buf);
          #endif
        #else
          //No DS3231 — the ESP32 system time is already set by configTzTime/SNTP
          #ifdef SHOW_SERIAL
            char buf[20];
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
            Serial.print(F("System time: ")); Serial.println(buf);
          #endif
        #endif

        // TODO send warning when battery is low ifdef BATTERY_MONITOR_PIN
        // TODO float battLevel = analogRead(35) / 4096.0 * 7.445;
        // if(battLevel < 4) {
        //   //TODO make web request to send warning about low battery while we have WiFi going
        // }
      } else {
        #ifdef SHOW_SERIAL
          Serial.println(F(" failed."));
        #endif
        #ifdef ENABLE_NEOPIXEL
          pixels.fill(0xFF5500); //orange - NTP failed (WiFi ok but no time)
          pixels.show();
          delay(1000);
        #endif
      }

      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
    } else {
      #ifdef ENABLE_NEOPIXEL
        pixels.fill(0xFF0000); //red - wifi failed
        pixels.show();
        delay(1000);
      #endif
      #ifdef SHOW_SERIAL
        Serial.println(F("WiFi connection failed."));
      #endif
    }
  #endif //NETWORK_SSID
}
