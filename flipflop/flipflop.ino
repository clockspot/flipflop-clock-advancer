// Low-power ESP32 + DS3231 solution for toggling a SPDT relay, for driving e.g. Solari Cifra 120
// https://github.com/clockspot/flipflop-clock-advancer
// Sketch by Luke McKenzie (luke@theclockspot.com)

#include <arduino.h>
#include "flipflop.h" //specifies config
#include <RTClib.h> //RTC_DS3231 and DateTime
#include "esp_sleep.h"

//TODO which of these are needed for NTP sync
// #include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson needs version v6 or above
// #include <WiFi.h>
// #include <WiFiClientSecure.h>
// #include <HTTPClient.h> // Needs to be from the ESP32 platform version 3.2.0 or later, as the previous has problems with http-redirect

//RTC objects
RTC_DS3231 rtc;
DateTime tod; //stores the rtc.now() snapshot for several functions to use

#ifdef ENABLE_NEOPIXEL
  #include <Adafruit_NeoPixel.h>
  #define NUMPIXELS 1
  Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
#endif

#ifndef RELAY_PULSE
#define RELAY_PULSE 10
#endif
#ifndef COLD_START_HOUR
#define COLD_START_HOUR 0
#endif

void setup(){

  pinMode(RELAY_UNSET_PIN, OUTPUT);
  pinMode(RELAY_SET_PIN, OUTPUT);
  gpio_hold_dis(RELAY_UNSET_PIN);
  gpio_hold_dis(RELAY_SET_PIN);
  digitalWrite(RELAY_UNSET_PIN,LOW);
  digitalWrite(RELAY_SET_PIN,LOW);

  pinMode(WAKEUP_PIN, INPUT_PULLUP);
  gpio_hold_en(WAKEUP_PIN); //https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html#_CPPv316rtc_gpio_hold_en10gpio_num_t
  esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, 0);
  //TODO save further power by leveraging Deep Sleep Wake Stub?
  //https://randomnerdtutorials.com/esp32-deep-sleep-arduino-ide-wake-up-sources/  
  
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
  
  //#ifdef SHOW_SERIAL
  Serial.begin(115200);
  // #ifdef SAMD_SERIES
  //   while(!Serial);
  // #else
  //   delay(1);
  // #endif
  // Serial.println(F("Hello world"));
  //#endif
  
  #ifdef DEV_STARTUP_PAUSE
    pixels.fill(0x0000FF); //indicate startup pause
    pixels.show();
    delay(3000); //for development, just in case it boot loops, this gives time to load a new sketch
  #endif
  
  rtc.begin();
  //RTC debug:
  // if(digitalRead(WAKEUP_PIN)) Serial.print("Pin high. "); else Serial.print("Pin low. ");
  // if(rtc.alarmFired(1)) Serial.print("Alarm 1 signaling. ");
  // if(rtc.alarmFired(2)) Serial.print("Alarm 2 signaling. ");
  
  //Who disturbs my slumber??
  if(esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_UNDEFINED) { //Cold start
  
    #ifdef ENABLE_NEOPIXEL
      pixels.fill(0x00FFFF); //teal to indicate hello world
      pixels.show();
    #endif
    //Setup RTC
    //For manual sync purposes, set the time (in CIFRA case, effectively the top of any even minute)
    rtc.adjust(DateTime(2020,1,1,COLD_START_HOUR,0,0));
    //Clear alarm signals, if present - do we need this here?
    // rtc.clearAlarm(1);
    // rtc.clearAlarm(2);
    //INT pin on DS3231 should be ready to go: "When INTCN is set to logic 1, then a match between the timekeeping registers and either of the alarm registers activates the INT/SQW pin (if the alarm is enabled). Because the INTCN bit is set to logic 1 when power is first applied, the pin defaults to an interrupt output with alarms disabled."
    //Set alarms
    #ifdef MODE_CIFRA
      rtc.setAlarm1(DateTime(2,0,10),DS3231_A1_Hour); //02:00:10: NTP/DST
      rtc.setAlarm2(DateTime(0,0,0),DS3231_A2_PerMinute); //every minute: toggle relay
    #endif
    #ifdef MODE_DATOR
      rtc.setAlarm1(DateTime(22,0,0),DS3231_A1_Hour); //22:00:00: set relay (warning), NTP/DST
      rtc.setAlarm2(DateTime(0,0,0),DS3231_A2_Hour); //00:00:00: unset relay (advance)
    #endif
    Serial.print("Clock set. ");
    //We do not advance the clock at this point, it is assumed to already be at position 00:00 / even minute / dator advance
    delay(10000); //gives a chance to upload new sketch before it sleeps
    
  } //end cold start
  else { //woke from sleep, probably by ESP_SLEEP_WAKEUP_EXT0
    //TODO could also maybe use esp_wake_deep_sleep
    
    tod = rtc.now(); //take snapshot of current time
    
    if(rtc.alarmFired(1)) {
      #ifdef MODE_DATOR
        //set relay for prepare state
        #ifdef ENABLE_NEOPIXEL
          pixels.fill(0xFF0000); //red
          pixels.show();
        #endif
        digitalWrite(RELAY_SET_PIN,HIGH);
        delay(RELAY_PULSE);
        digitalWrite(RELAY_SET_PIN,LOW);
      #endif
      //for both CIFRA and DATOR
      rtcAdjust();
    } //end alarmFired(1)
    
    if(rtc.alarmFired(2)) {
      #ifdef MODE_CIFRA //Toggle relay per RTC minutes
        if(tod.minute()%2) { //set relay for odd minute
          #ifdef ENABLE_NEOPIXEL
            pixels.fill(0xFF0000); //red
            pixels.show();
          #endif
          digitalWrite(RELAY_SET_PIN,HIGH);
          delay(RELAY_PULSE);
          digitalWrite(RELAY_SET_PIN,LOW);  
        } else { //unset relay for even minute
          #ifdef ENABLE_NEOPIXEL
            pixels.fill(0x00FF00); //green
            pixels.show();
          #endif
          digitalWrite(RELAY_UNSET_PIN,HIGH);
          delay(RELAY_PULSE);
          digitalWrite(RELAY_UNSET_PIN,LOW);
        }
      #endif
      #ifdef MODE_DATOR
        //unset relay for advance state
        #ifdef ENABLE_NEOPIXEL
          pixels.fill(0x00FF00); //green
          pixels.show();
        #endif
        digitalWrite(RELAY_UNSET_PIN,HIGH);
        delay(RELAY_PULSE);
        digitalWrite(RELAY_UNSET_PIN,LOW);
      #endif
    } //end alarmFired(2)
    
  } //end woke from sleep
  
  //RTC debug:
  // //Clear alarm signals
  // rtc.clearAlarm(1);
  // rtc.clearAlarm(2);
  // Serial.print("After clear, ");
  // if(digitalRead(WAKEUP_PIN)) Serial.print("Pin high. "); else Serial.print("Pin low. ");
  // tod = rtc.now();
  // Serial.print("Current time is ");
  // if(tod.hour()<10) Serial.print("0"); Serial.print(tod.hour(),DEC); Serial.print(":");
  // if(tod.minute()<10) Serial.print("0"); Serial.print(tod.minute(),DEC); Serial.print(":");
  // if(tod.second()<10) Serial.print("0"); Serial.print(tod.second(),DEC);
  // Serial.print(". ");
  // if(rtc.alarmFired(1)) Serial.print("Alarm 1 still signaling. ");
  // if(rtc.alarmFired(2)) Serial.print("Alarm 2 still signaling. ");
  // Serial.println();
  
} //end setup()

void loop() {
  //Once setup is done, quiet down and go to sleep
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  digitalWrite(RELAY_UNSET_PIN,LOW);
  digitalWrite(RELAY_SET_PIN,LOW);
  gpio_hold_en(RELAY_UNSET_PIN);
  gpio_hold_en(RELAY_SET_PIN);
  Serial.flush(); 
  esp_deep_sleep_start();
}

void rtcAdjust() {
  #ifdef ENABLE_NTP_SYNC
    //TODO sync NTP
    //TODO adjust for DST: if current day involves a change, adjust RTC accordingly; CIFRA should advance/wait display 1hr
    // //Start wifi TODO adapt to ESP32 QT Py
    // for(int attempts=0; attempts<3; attempts++) {
    //   Serial.print(F("\nConnecting to WiFi SSID "));
    //   Serial.println(NETWORK_SSID);
    //   WiFi.begin(NETWORK_SSID, NETWORK_PASS);
    //   int timeout = 0;
    //   while(WiFi.status()!=WL_CONNECTED && timeout<15) {
    //     timeout++; delay(1000);
    //   }
    //   if(WiFi.status()==WL_CONNECTED){ //did it work?
    //     //Serial.print(millis());
    //     Serial.println(F("Connected!"));
    //     //Serial.print(F("SSID: ")); Serial.println(WiFi.SSID());
    //     Serial.print(F("Signal strength (RSSI): ")); Serial.print(WiFi.RSSI()); Serial.println(F(" dBm"));
    //     Serial.print(F("Local IP: ")); Serial.println(WiFi.localIP());
    //     break; //leave attempts loop
    //   } else {
    //     // #ifdef NETWORK2_SSID
    //     //   Serial.print(F("\nConnecting to WiFi SSID "));
    //     //   Serial.println(NETWORK2_SSID);
    //     //   WiFi.begin(NETWORK2_SSID, NETWORK2_PASS);
    //     //   int timeout = 0;
    //     //   while(WiFi.status()!=WL_CONNECTED && timeout<15) {
    //     //     timeout++; delay(1000);
    //     //   }
    //     //   if(WiFi.status()==WL_CONNECTED){ //did it work?
    //     //     //Serial.print(millis());
    //     //     Serial.println(F("Connected!"));
    //     //     //Serial.print(F("SSID: ")); Serial.println(WiFi.SSID());
    //     //     Serial.print(F("Signal strength (RSSI): ")); Serial.print(WiFi.RSSI()); Serial.println(F(" dBm"));
    //     //     Serial.print(F("Local IP: ")); Serial.println(WiFi.localIP());
    //     //     break; //leave attempts loop
    //     //   }
    //     // #endif
    //   }
    // }
    // if(WiFi.status()!=WL_CONNECTED) {
    //   Serial.println(F("Wasn't able to connect."));
    //   displayError(F("Couldn't connect to WiFi."));
    //   //Close unneeded things
    //   WiFi.disconnect(true);
    //   WiFi.mode(WIFI_OFF);
    //   return;
    // }
    // 
    // // Get time from timeserver - used when going into deep sleep again to ensure that we wake at the right hour
    // struct tm timeinfo; //may need to be outside fn
    // //(these are part of the espressif arduino-esp32 core)
    // configTime(TZ_OFFSET_SEC, DST_OFFSET_SEC, NTP_HOST);
    // if(!getLocalTime(&timeinfo)){
    //   Serial.println("Failed to obtain time");
    // }
    // //example usage
    // //todInSec = timeinfo.tm_hour*60*60 + timeinfo.tm_min*60 + timeinfo.tm_sec;
    // 
    // //rtcTakeSnap
    // //rtcGet functions pull from this snapshot - to ensure that code works off the same timestamp
    // tod = rtc.now();
    // 
    // //rtcSetTime
    // rtcTakeSnap();
    // rtc.adjust(DateTime(tod.year(),tod.month(),tod.day(),h,m,s));
    // 
    // //rtcSetDate
    // //will cause the clock to fall slightly behind since it discards partial current second
    // rtcTakeSnap();
    // rtc.adjust(DateTime(y,m,d,tod.hour(),tod.minute(),tod.second()));
    // 
    // //rtcSetHour
    // //will cause the clock to fall slightly behind since it discards partial current second
    // rtcTakeSnap();
    // rtc.adjust(DateTime(tod.year(),tod.month(),tod.day(),h,tod.minute(),tod.second()));
    // 
    //TODO send warning when battery is low ifdef BATTERY_MONITOR_PIN
    //TODO float battLevel = analogRead(35) / 4096.0 * 7.445;
    // if(battLevel < 4) {
    //   //TODO make web request to send warning about low battery while we have WiFi going
    //   HTTPClient http; //may need to be outside fn
    //   //Get data and attempt to parse it
    //   //This can fail two ways: httpReturnCode != 200, or parse fails
    //   //In either case, we will attempt to pull it anew
    //   int httpReturnCode;
    //   bool parseSuccess = false;
    // 
    //   for(int attempts=0; attempts<3; attempts++) {
    //     Serial.print(F("\nConnecting to data source "));
    //     Serial.print(DATA_SRC);
    //     Serial.print(F(" at tod "));
    //     Serial.println(todInSec,DEC);
    // //     std::string targetURL(DATA_SRC);
    // //     std::string todInSecStr = std::to_string(todInSec);
    //     http.begin(String(DATA_SRC)+"&tod="+String(todInSec));
    //     httpReturnCode = http.GET();
    //     if(httpReturnCode==200) { //got data, let's try to parse
    //       //hooray
    //     }
    //   }
    // } //end if batt level too low
    // 
    // //Close unneeded things
    // http.end();
    // WiFi.disconnect(true);
    // WiFi.mode(WIFI_OFF);
  #endif
}