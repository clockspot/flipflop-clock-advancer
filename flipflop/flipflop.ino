// Low-power ESP32 + DS3231 solution for toggling a SPDT relay, for driving e.g. Solari Cifra 120
// https://github.com/clockspot/flipflop-clock-advancer
// Sketch by Luke McKenzie (luke@theclockspot.com)

#include <arduino.h>
#include "flipflop.h" //specifies config
//#include <Wire.h> //Arduino - GNU LPGL - for I2C access to DS3231
#include <RTClib.h> //RTC_DS3231 and DateTime

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

void setup(){

  pinMode(RELAY_UNSET_PIN, OUTPUT);
  pinMode(RELAY_SET_PIN, OUTPUT);
  #ifdef BATTERY_MONITOR_PIN
    pinMode(BATTERY_MONITOR_PIN, INPUT);
  #endif
  digitalWrite(RELAY_UNSET_PIN,LOW);
  digitalWrite(RELAY_SET_PIN,LOW);

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
  
    pixels.fill(0x0000FF); //start blue
    pixels.show();
  #endif
  
  //#ifdef SHOW_SERIAL
  Serial.begin(115200);
//   #ifdef SAMD_SERIES
//     while(!Serial);
//   #else
//     delay(1);
//   #endif
//   Serial.println(F("Hello world"));
  //#endif
  
  #ifdef DEV_STARTUP_PAUSE
    delay(5000); //for development, just in case it boot loops
  #endif
  Serial.println("Hello world");
  
  //Wire.begin();
  rtc.begin();
  
//   #ifdef DEV_STAY_AWAKE
//     //setAlarms();
//   #else //normal
//     //Who disturbs my slumber (if anything)?
//     if(esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_UNDEFINED) { //or perhaps  !=ESP_SLEEP_WAKEUP_EXT0
//       //Manual powerup or reset
//       //For manual sync purposes, assume 00:00:00 local time (in CIFRA case, effectively the top of any even minute)
//       rtc.adjust(DateTime(2020,1,1,0,0,0));
//       //Set alarms
//       #ifdef ENABLE_NEOPIXEL
//         pixels.fill(0x00FFFF); //teal to indicate manual RTC set
//         pixels.show();
//       #endif
//     } else { //TODO could also maybe use esp_wake_deep_sleep
//       if(rtc.alarmFired(1)) {
//         #ifdef MODE_DATOR
//           //TODO set relay (warning)
//           #ifdef ENABLE_NEOPIXEL
//             pixels.fill(0xFF0000); //red to indicate relay set
//             pixels.show();
//           #endif
//         #endif
//         //for both CIFRA and DATOR
//         //TODO sync NTP
//         //TODO adjust for DST: if current day involves a change, adjust RTC accordingly; CIFRA should advance/wait display 1hr
//         
//         if(ENABLE_NTP_SYNC) { //TODO trigger from DS3231 alarm    
//           //Start wifi TODO adapt to ESP32 QT Py
//           for(int attempts=0; attempts<3; attempts++) {
//             Serial.print(F("\nConnecting to WiFi SSID "));
//             Serial.println(NETWORK_SSID);
//             WiFi.begin(NETWORK_SSID, NETWORK_PASS);
//             int timeout = 0;
//             while(WiFi.status()!=WL_CONNECTED && timeout<15) {
//               timeout++; delay(1000);
//             }
//             if(WiFi.status()==WL_CONNECTED){ //did it work?
//               //Serial.print(millis());
//               Serial.println(F("Connected!"));
//               //Serial.print(F("SSID: ")); Serial.println(WiFi.SSID());
//               Serial.print(F("Signal strength (RSSI): ")); Serial.print(WiFi.RSSI()); Serial.println(F(" dBm"));
//               Serial.print(F("Local IP: ")); Serial.println(WiFi.localIP());
//               break; //leave attempts loop
//             } else {
//               // #ifdef NETWORK2_SSID
//               //   Serial.print(F("\nConnecting to WiFi SSID "));
//               //   Serial.println(NETWORK2_SSID);
//               //   WiFi.begin(NETWORK2_SSID, NETWORK2_PASS);
//               //   int timeout = 0;
//               //   while(WiFi.status()!=WL_CONNECTED && timeout<15) {
//               //     timeout++; delay(1000);
//               //   }
//               //   if(WiFi.status()==WL_CONNECTED){ //did it work?
//               //     //Serial.print(millis());
//               //     Serial.println(F("Connected!"));
//               //     //Serial.print(F("SSID: ")); Serial.println(WiFi.SSID());
//               //     Serial.print(F("Signal strength (RSSI): ")); Serial.print(WiFi.RSSI()); Serial.println(F(" dBm"));
//               //     Serial.print(F("Local IP: ")); Serial.println(WiFi.localIP());
//               //     break; //leave attempts loop
//               //   }
//               // #endif
//             }
//           }
//           if(WiFi.status()!=WL_CONNECTED) {
//             Serial.println(F("Wasn't able to connect."));
//             displayError(F("Couldn't connect to WiFi."));
//             //Close unneeded things
//             WiFi.disconnect(true);
//             WiFi.mode(WIFI_OFF);
//             return;
//           }
//           
//           // Get time from timeserver - used when going into deep sleep again to ensure that we wake at the right hour
//           struct tm timeinfo; //may need to be outside fn
//           //(these are part of the espressif arduino-esp32 core)
//           configTime(TZ_OFFSET_SEC, DST_OFFSET_SEC, NTP_HOST);
//           if(!getLocalTime(&timeinfo)){
//             Serial.println("Failed to obtain time");
//           }
//           //example usage
//           //todInSec = timeinfo.tm_hour*60*60 + timeinfo.tm_min*60 + timeinfo.tm_sec;
//           
//           //rtcTakeSnap
//           //rtcGet functions pull from this snapshot - to ensure that code works off the same timestamp
//           tod = rtc.now();
//           
//           //rtcSetTime
//           rtcTakeSnap();
//           rtc.adjust(DateTime(tod.year(),tod.month(),tod.day(),h,m,s));
//           
//           //rtcSetDate
//           //will cause the clock to fall slightly behind since it discards partial current second
//           rtcTakeSnap();
//           rtc.adjust(DateTime(y,m,d,tod.hour(),tod.minute(),tod.second()));
//           
//           //rtcSetHour
//           //will cause the clock to fall slightly behind since it discards partial current second
//           rtcTakeSnap();
//           rtc.adjust(DateTime(tod.year(),tod.month(),tod.day(),h,tod.minute(),tod.second()));
//           
//           if(battLevel < 4) {
//             //TODO make web request to send warning about low battery while we have WiFi going
//             HTTPClient http; //may need to be outside fn
//             //Get data and attempt to parse it
//             //This can fail two ways: httpReturnCode != 200, or parse fails
//             //In either case, we will attempt to pull it anew
//             int httpReturnCode;
//             bool parseSuccess = false;
//           
//             for(int attempts=0; attempts<3; attempts++) {
//               Serial.print(F("\nConnecting to data source "));
//               Serial.print(DATA_SRC);
//               Serial.print(F(" at tod "));
//               Serial.println(todInSec,DEC);
//           //     std::string targetURL(DATA_SRC);
//           //     std::string todInSecStr = std::to_string(todInSec);
//               http.begin(String(DATA_SRC)+"&tod="+String(todInSec));
//               httpReturnCode = http.GET();
//               if(httpReturnCode==200) { //got data, let's try to parse
//                 //hooray
//               }
//             }
//           } //end if batt level too low
//           
//           //Close unneeded things
//           http.end();
//           WiFi.disconnect(true);
//           WiFi.mode(WIFI_OFF);
//         } //end if ENABLE_NTP_SYNC
//         
//       } else if(rtc.alarmFired(2)) {
//         #ifdef MODE_CIFRA //Toggle relay per RTC minutes
//           //TODO toggle relay per getMins()%2: 0=unset, 1=set
//           #ifdef ENABLE_NEOPIXEL
//             if(1) pixels.fill(0xFF0000); //red to indicate relay set
//             else  pixels.fill(0x00FF00); //green to indicate relay unset
//             pixels.show();
//           #endif
//         #endif
//         #ifdef MODE_DATOR
//           //TODO unset relay (advance)
//           #ifdef ENABLE_NEOPIXEL
//             pixels.fill(0x00FF00); //green to indicate relay unset
//             pixels.show();
//           #endif
//         #endif
//       }
//       //else what woke us up?? it must have been nothing, go back to bed.
//     } //end who disturbs my slumber
//   #endif //normal function
  
  //TODO send warning when battery is low
  //TODO float battLevel = analogRead(35) / 4096.0 * 7.445;

} //end setup()

void loop() {
//   Serial.println("hello world");
  #ifdef DEV_STAY_AWAKE
    rtcCheck();
  #else //normal function
    //whenever the rest of the code is done, sleep
    #ifdef ENABLE_NEOPIXEL
      delay(1000); //to give some time to see it
    #endif
    Serial.flush(); 
    esp_deep_sleep_start();
  #endif
}

void rtcSetAlarms() {
  //Clear RTC alarm signals, if present
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  //Set alarms
  #ifdef MODE_CIFRA
    rtc.setAlarm1(DateTime(2,0,10),DS3231_A1_Second); //02:00:10: NTP/DST
    rtc.setAlarm2(DateTime(0,0,0),DS3231_A2_PerMinute); //every minute: toggle relay
  #endif
  #ifdef MODE_DATOR
    rtc.setAlarm1(DateTime(22,0,0),DS3231_A1_Second); //22:00:00: set relay (warning), NTP/DST
    rtc.setAlarm2(DateTime(0,0,0),DS3231_A2_Second); //00:00:00: unset relay (advance)
  #endif
}

//assumes MODE_CIFRA and ENABLE_NEOPIXEL and DEV_STAY_AWAKE
byte rtcSecLast = 61;
void rtcCheck() {
  //tod = rtc.now();
  byte rtcSec = (millis()/1000)%60;
  if(rtcSecLast != rtcSec) { //tod.second()
    rtcSecLast = rtcSec; //tod.second();
//     Serial.print("rtcSecond: ");
//     Serial.print(rtcSecLast,DEC);
    byte rtcSecNow = rtcSecLast%5;
    byte rtcMinNow = (rtcSecLast/5)%2; //0=0, 5=1, 10=0...
//     Serial.print("  rtcSecNow: ");
//     Serial.print(rtcSecNow,DEC);
//     Serial.print("  rtcMinNow: ");
//     Serial.print(rtcMinNow,DEC);
    if(rtcSecNow == 0) {
      //Serial.println("Go!");
      pixels.fill(0x00FF00);
      pixels.show();
      if(rtcMinNow==0) {
        digitalWrite(RELAY_UNSET_PIN,HIGH);
      } else {
        digitalWrite(RELAY_SET_PIN,HIGH);
      }
      delay(200);
      pixels.clear();
      pixels.show();
      digitalWrite(RELAY_UNSET_PIN,LOW);
      digitalWrite(RELAY_SET_PIN,LOW);
    }
    else { //if(rtcSecNow < 3) {
      //blink red
      //Serial.println("Ready");
      pixels.fill(0x0000FF);
      pixels.show();
      delay(150);
      pixels.clear();
      pixels.show();
    }
//     else {
//       //blink yellow
//       //Serial.println("Steady");
//       pixels.fill(0xFFFF00);
//       pixels.show();
//       delay(200);
//       pixels.clear();
//       pixels.show();
//     }
//     Serial.println();
  }
}