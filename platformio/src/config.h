#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <cstdint>
#include <vector>
#include <Arduino.h>

// AIR QUALITY INDEX
// Seemingly every country uses a different scale for Air Quality Index (AQI).
// I have written a library to calculate many of the most popular AQI scales.
// Feel free to request the addition of a new AQI scale by opening an Issue.
// https://github.com/lmarzen/pollutant-concentration-to-aqi
// Uncomment your preferred AQI scale.
// #define AUSTRALIA_AQI
// #define CANADA_AQHI
// #define EUROPE_CAQI
// #define HONG_KONG_AQHI
// #define INDIA_AQI
// #define MAINLAND_CHINA_AQI
// #define SINGAPORE_PSI
// #define SOUTH_KOREA_CAI
// #define UNITED_KINGDOM_DAQI
#define UNITED_STATES_AQI

// Set the below variables in "config.cpp"
namespace config
{
  extern const uint8_t PIN_BAT_ADC;
  extern const uint8_t PIN_EPD_BUSY;
  extern const uint8_t PIN_EPD_CS;
  extern const uint8_t PIN_EPD_RST;
  extern const uint8_t PIN_EPD_DC;
  extern const uint8_t PIN_EPD_SCK;
  extern const uint8_t PIN_EPD_MISO;
  extern const uint8_t PIN_EPD_MOSI;
  extern const char* WIFI_SSID;
  extern const char* WIFI_PASSWORD;
  extern const String OWM_APIKEY;
  extern const String OWM_ENDPOINT;
  extern const String LAT;
  extern const String LON;
  extern const String CITY;
  extern const char* TIMEZONE;
  extern const char* NTP_SERVER_1;
  extern const char* NTP_SERVER_2;
  extern const long SLEEP_DUR;
  extern const int  WAKE_TIME;
  extern const int  BED_TIME;
  extern const String LANG;
  extern const char UNITS;
  extern const int HOURLY_GRAPH_MAX;
  extern const std::vector<String> ALERT_URGENCY;
}

#endif