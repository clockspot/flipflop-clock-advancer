// Low-power ESP32 + DS3231 solution for toggling a SPDT relay, for driving e.g. Solari Cifra 120
// https://github.com/clockspot/flipflop-clock-advancer
// Sketch by Luke McKenzie (luke@theclockspot.com)

#include <arduino.h>
#include "flipflop.h" //specifies config

//TODO which of these are needed for NTP sync
// #include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson needs version v6 or above
// #include <WiFi.h>
// #include <WiFiClientSecure.h>
// #include <HTTPClient.h> // Needs to be from the ESP32 platform version 3.2.0 or later, as the previous has problems with http-redirect

//Stopping place: bring in stuff from arduino-clock/rtcDS3231.cpp

#ifdef ENABLE_NEOPIXEL
  #include <Adafruit_NeoPixel.h>
  #define NUMPIXELS 1
  Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
#endif

void setup(){
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
  
    pixels.fill(0xFFFF00); //start yellow
    pixels.show();
  #endif
  
  #ifdef DEV_STARTUP_PAUSE
    delay(5000); //for development, just in case it boot loops
  #endif
  
  if(SHOW_SERIAL) {
    Serial.begin(115200);
    #ifdef SAMD_SERIES
      while(!Serial);
    #else
      delay(1);
    #endif
    Serial.println(F("Hello world"));
  }
  
  //TODO float battLevel = analogRead(35) / 4096.0 * 7.445;
  
  if(timeToSyncNTP) { //TODO trigger from DS3231 alarm
    
    //Start wifi TODO adapt to ESP32 QT Py
    for(int attempts=0; attempts<3; attempts++) {
      Serial.print(F("\nConnecting to WiFi SSID "));
      Serial.println(NETWORK_SSID);
      WiFi.begin(NETWORK_SSID, NETWORK_PASS);
      int timeout = 0;
      while(WiFi.status()!=WL_CONNECTED && timeout<15) {
        timeout++; delay(1000);
      }
      if(WiFi.status()==WL_CONNECTED){ //did it work?
        //Serial.print(millis());
        Serial.println(F("Connected!"));
        //Serial.print(F("SSID: ")); Serial.println(WiFi.SSID());
        Serial.print(F("Signal strength (RSSI): ")); Serial.print(WiFi.RSSI()); Serial.println(F(" dBm"));
        Serial.print(F("Local IP: ")); Serial.println(WiFi.localIP());
        break; //leave attempts loop
      } else {
        // #ifdef NETWORK2_SSID
        //   Serial.print(F("\nConnecting to WiFi SSID "));
        //   Serial.println(NETWORK2_SSID);
        //   WiFi.begin(NETWORK2_SSID, NETWORK2_PASS);
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
        //   }
        // #endif
      }
    }
    if(WiFi.status()!=WL_CONNECTED) {
      Serial.println(F("Wasn't able to connect."));
      displayError(F("Couldn't connect to WiFi."));
      //Close unneeded things
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      return;
    }
  
    // Get time from timeserver - used when going into deep sleep again to ensure that we wake at the right hour
    struct tm timeinfo; //may need to be outside fn
    //(these are part of the espressif arduino-esp32 core)
    configTime(TZ_OFFSET_SEC, DST_OFFSET_SEC, NTP_HOST);
    if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
    }
    //example usage
    //todInSec = timeinfo.tm_hour*60*60 + timeinfo.tm_min*60 + timeinfo.tm_sec;
    
    if(battLevel < 4) {
      //TODO make web request to send warning about low battery while we have WiFi going
      HTTPClient http; //may need to be outside fn
      //Get data and attempt to parse it
      //This can fail two ways: httpReturnCode != 200, or parse fails
      //In either case, we will attempt to pull it anew
      int httpReturnCode;
      bool parseSuccess = false;
    
      for(int attempts=0; attempts<3; attempts++) {
        Serial.print(F("\nConnecting to data source "));
        Serial.print(DATA_SRC);
        Serial.print(F(" at tod "));
        Serial.println(todInSec,DEC);
    //     std::string targetURL(DATA_SRC);
    //     std::string todInSecStr = std::to_string(todInSec);
        http.begin(String(DATA_SRC)+"&tod="+String(todInSec));
        httpReturnCode = http.GET();
        if(httpReturnCode==200) { //got data, let's try to parse
          //hooray
        }
      }
    } //end if batt level too low
    
    //Close unneeded things
    http.end();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  } //end timeToSyncNTP
  
  rtcInit();
  //TODO ensure its alarms are set - maybe after checking for which alarm woke it up?
  //for 2-minute cycle, AL1 at e.g. 23:45:10 for NTP sync, AL2 at every minute and alternating trigger
  //for daily cycle, AL1 at e.g. 23:00:00 for NTP sync and trigger A; AL2 at 00:00:00 for trigger B
  
  //TODO initStorage();
  //optional persistent storage of currently displayed time, to assist with power recovery, DST adjusts, manual adjusts(?)
  
  //TODO init output pins for relay
  
  //Prepare for next time:
  //TODO replace this with DS3231 wakeups
  
  //TODO If battery is too low, enter deepSleep and do not wake up
  if(battLevel == 0) {
    esp_deep_sleep_start();
  }
  if(todInSec>43200) { //we're early: it's PM - example: (86400*2)-80852 = 91948
    esp_sleep_enable_timer_wakeup((86400*2 - todInSec) * 1000000ULL);  
  } else { //we're late: it's AM - example: 86400-3600 = 82800
    esp_sleep_enable_timer_wakeup((86400 - todInSec) * 1000000ULL);  
  }
  
} //end setup()

void loop() {
  //whenever the rest of the code is done, sleep
  Serial.flush(); 
  esp_deep_sleep_start();
}