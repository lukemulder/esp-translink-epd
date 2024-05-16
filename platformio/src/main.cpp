/* Main program for esp32-weather-epd.
 * Copyright (C) 2022-2024  Luke Marzen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <Preferences.h>
#include <time.h>
#include <WiFi.h>
#include <Wire.h>

#include "_locale.h"
#include "api_response.h"
#include "client_utils.h"
#include "config.h"
#include "display_utils.h"
#include "icons/icons_196x196.h"
#include "renderer.h"
#include "data_manager.h"
#if defined(USE_HTTPS_WITH_CERT_VERIF) || defined(USE_HTTPS_WITH_CERT_VERIF)
  #include <WiFiClientSecure.h>
#endif
#ifdef USE_HTTPS_WITH_CERT_VERIF
  #include "cert.h"
#endif

Preferences prefs;

/* Put esp32 into ultra low-power deep sleep (<11μA).
 * Aligns wake time to the minute. Sleep times defined in config.cpp.
 */
void beginDeepSleep(unsigned long &startTime, tm *timeInfo)
{
  if (!getLocalTime(timeInfo))
  {
    Serial.println(TXT_REFERENCING_OLDER_TIME_NOTICE);
  }

  uint64_t sleepDuration = 0;
  int extraHoursUntilWake = 0;
  int curHour = timeInfo->tm_hour;

  if (timeInfo->tm_min >= 58)
  { // if we are within 2 minutes of the next hour, then round up for the
    // purposes of bed time
    curHour = (curHour + 1) % 24;
    extraHoursUntilWake += 1;
  }

  if (BED_TIME < WAKE_TIME && curHour >= BED_TIME && curHour < WAKE_TIME)
  { // 0              B   v  W  24
    // |--------------zzzzZzz---|
    extraHoursUntilWake += WAKE_TIME - curHour;
  }
  else if (BED_TIME > WAKE_TIME && curHour < WAKE_TIME)
  { // 0 v W               B    24
    // |zZz----------------zzzzz|
    extraHoursUntilWake += WAKE_TIME - curHour;
  }
  else if (BED_TIME > WAKE_TIME && curHour >= BED_TIME)
  { // 0   W               B  v 24
    // |zzz----------------zzzZz|
    extraHoursUntilWake += WAKE_TIME - (curHour - 24);
  }
  else // This feature is disabled (BED_TIME == WAKE_TIME)
  {    // OR it is not past BED_TIME
    extraHoursUntilWake = 0;
  }

  if (extraHoursUntilWake == 0)
  { // align wake time to nearest multiple of SLEEP_DURATION
    sleepDuration = SLEEP_DURATION * 60ULL
                    - ((timeInfo->tm_min % SLEEP_DURATION) * 60ULL
                        + timeInfo->tm_sec);
  }
  else
  { // align wake time to the hour
    sleepDuration = extraHoursUntilWake * 3600ULL
                    - (timeInfo->tm_min * 60ULL + timeInfo->tm_sec);
  }

  // if we are within 2 minutes of the next alignment.
  if (sleepDuration <= 120ULL)
  {
    sleepDuration += SLEEP_DURATION * 60ULL;
  }

  // add extra delay to compensate for esp32's with fast RTCs.
  sleepDuration += 10ULL;

#if DEBUG_LEVEL >= 1
  printHeapUsage();
#endif

  esp_sleep_enable_timer_wakeup(sleepDuration * 1000000ULL);
  Serial.print(TXT_AWAKE_FOR);
  Serial.println(" "  + String((millis() - startTime) / 1000.0, 3) + "s");
  Serial.print(TXT_ENTERING_DEEP_SLEEP_FOR);
  Serial.println(" " + String(sleepDuration) + "s");
  esp_deep_sleep_start();
} // end beginDeepSleep

/* Program entry point.
 */
void setup()
{
  unsigned long startTime = millis();
  Serial.begin(115200);

#if DEBUG_LEVEL >= 1
  printHeapUsage();
#endif

  disableBuiltinLED();

  // Open namespace for read/write to non-volatile storage
  prefs.begin(NVS_NAMESPACE, false);

#if BATTERY_MONITORING
  uint32_t batteryVoltage = readBatteryVoltage();
  Serial.print(TXT_BATTERY_VOLTAGE);
  Serial.println(": " + String(batteryVoltage) + "mv");

  // When the battery is low, the display should be updated to reflect that, but
  // only the first time we detect low voltage. The next time the display will
  // refresh is when voltage is no longer low. To keep track of that we will
  // make use of non-volatile storage.
  bool lowBat = prefs.getBool("lowBat", false);

  // low battery, deep sleep now
  if (batteryVoltage <= LOW_BATTERY_VOLTAGE)
  {
    if (lowBat == false)
    { // battery is now low for the first time
      prefs.putBool("lowBat", true);
      prefs.end();
      initDisplay();
      do
      {
        drawError(battery_alert_0deg_196x196, TXT_LOW_BATTERY);
      } while (display.nextPage());
      powerOffDisplay();
    }

    if (batteryVoltage <= CRIT_LOW_BATTERY_VOLTAGE)
    { // critically low battery
      // don't set esp_sleep_enable_timer_wakeup();
      // We won't wake up again until someone manually presses the RST button.
      Serial.println(TXT_CRIT_LOW_BATTERY_VOLTAGE);
      Serial.println(TXT_HIBERNATING_INDEFINITELY_NOTICE);
    }
    else if (batteryVoltage <= VERY_LOW_BATTERY_VOLTAGE)
    { // very low battery
      esp_sleep_enable_timer_wakeup(VERY_LOW_BATTERY_SLEEP_INTERVAL
                                    * 60ULL * 1000000ULL);
      Serial.println(TXT_VERY_LOW_BATTERY_VOLTAGE);
      Serial.print(TXT_ENTERING_DEEP_SLEEP_FOR);
      Serial.println(" " + String(VERY_LOW_BATTERY_SLEEP_INTERVAL) + "min");
    }
    else
    { // low battery
      esp_sleep_enable_timer_wakeup(LOW_BATTERY_SLEEP_INTERVAL
                                    * 60ULL * 1000000ULL);
      Serial.println(TXT_LOW_BATTERY_VOLTAGE);
      Serial.print(TXT_ENTERING_DEEP_SLEEP_FOR);
      Serial.println(" " + String(LOW_BATTERY_SLEEP_INTERVAL) + "min");
    }
    esp_deep_sleep_start();
  }
  // battery is no longer low, reset variable in non-volatile storage
  if (lowBat == true)
  {
    prefs.putBool("lowBat", false);
  }
#else
  uint32_t batteryVoltage = UINT32_MAX;
#endif

  // All data should have been loaded from NVS. Close filesystem.
  prefs.end();

  String statusStr = {};
  String tmpStr = {};
  tm timeInfo = {};

  bool firstBoot = (esp_reset_reason() != ESP_RST_DEEPSLEEP);
  bool timeConfigured = false;

  setenv("TZ", TIMEZONE, 1);
  getLocalTime(&timeInfo);

  DataManager dataManager;

  dataManager.init(&timeInfo, firstBoot);

  int wifiRSSI = 0; // “Received Signal Strength Indicator"

  if(firstBoot || dataManager.openWeatherMapDataStale() || dataManager.translinkDataStale())
  {
    // START WIFI
    wl_status_t wifiStatus = startWiFi(wifiRSSI);
    if (wifiStatus != WL_CONNECTED)
    { // WiFi Connection Failed
      killWiFi();
      initDisplay();
      if (wifiStatus == WL_NO_SSID_AVAIL)
      {
        Serial.println(TXT_NETWORK_NOT_AVAILABLE);
        do
        {
          drawError(wifi_x_196x196, TXT_NETWORK_NOT_AVAILABLE);
        } while (display.nextPage());
      }
      else
      {
        Serial.println(TXT_WIFI_CONNECTION_FAILED);
        do
        {
          drawError(wifi_x_196x196, TXT_WIFI_CONNECTION_FAILED);
        } while (display.nextPage());
      }
      powerOffDisplay();
      beginDeepSleep(startTime, &timeInfo);
    }

    // TIME SYNCHRONIZATION
    configTzTime(TIMEZONE, NTP_SERVER_1, NTP_SERVER_2);
    timeConfigured = waitForSNTPSync(&timeInfo);
    if (!timeConfigured)
    {
      Serial.println(TXT_TIME_SYNCHRONIZATION_FAILED);
      killWiFi();
      initDisplay();
      do
      {
        drawError(wi_time_4_196x196, TXT_TIME_SYNCHRONIZATION_FAILED);
      } while (display.nextPage());
      powerOffDisplay();
      beginDeepSleep(startTime, &timeInfo);
    }

    dataManager.setTimeInfo(&timeInfo);
  }

  // MAKE API REQUESTS
#ifdef USE_HTTP
  WiFiClient client;
#elif defined(USE_HTTPS_NO_CERT_VERIF)
  WiFiClientSecure client;
  client.setInsecure();
#elif defined(USE_HTTPS_WITH_CERT_VERIF)
  WiFiClientSecure client;
  client.setCACert(cert_Sectigo_RSA_Domain_Validation_Secure_Server_CA);
#endif

  int rxStatus;

  // WEATHER ONE CALL
  if(dataManager.openWeatherMapDataStale())
  {
    rxStatus = dataManager.updateOpenWeatherMapData();
    if (rxStatus != HTTP_CODE_OK)
    {
      killWiFi();
      statusStr = "One Call " + OWM_ONECALL_VERSION + " API";
      tmpStr = String(rxStatus, DEC) + ": " + getHttpResponsePhrase(rxStatus);
      initDisplay();
      do
      {
        drawError(wi_cloud_down_196x196, statusStr, tmpStr);
      } while (display.nextPage());
      powerOffDisplay();
      beginDeepSleep(startTime, &timeInfo);
    }
  }

  // TRANSLINK API CALL
  if(dataManager.translinkDataStale())
  {
    rxStatus = dataManager.updateTranslinkData();
    if (rxStatus != HTTP_CODE_OK)
    {
      killWiFi();
      statusStr = "Translink RTTI API";
      tmpStr = String(rxStatus, DEC) + ": " + getHttpResponsePhrase(rxStatus);
      initDisplay();
      do
      {
        drawError(wi_cloud_down_196x196, statusStr, tmpStr);
      } while (display.nextPage());
      powerOffDisplay();
      beginDeepSleep(startTime, &timeInfo);
    }
  }

  killWiFi(); // WiFi no longer needed

  String refreshTimeStr;
  getRefreshTimeStr(refreshTimeStr, timeConfigured, &timeInfo);
  String dateStr;
  getDateStr(dateStr, &timeInfo);

  // RENDER DISPLAY REFRESH
  initDisplay();

  do
  {
    drawCurrentConditionsTopBar(*dataManager.getOpenWeatherMapData());
    drawForecastTopBar(*dataManager.getOpenWeatherMapData(), timeInfo);
    drawLocationDate(CITY_STRING, dateStr);
    drawCurrentTime(timeInfo);
    
    drawBusSchedules(dataManager.getTranslinkData());
    drawStatusBar(statusStr, refreshTimeStr, wifiRSSI, batteryVoltage);
  } while (display.nextPage());
  powerOffDisplay();

  // DEEP SLEEP
  beginDeepSleep(startTime, &timeInfo);
} // end setup

/* This will never run
 */
void loop()
{
} // end loop

