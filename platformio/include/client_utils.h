/* Client side utility declarations for esp32-weather-epd.
 * Copyright (C) 2022-2023  Luke Marzen
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

#ifndef __CLIENT_UTILS_H__
#define __CLIENT_UTILS_H__

#include <Arduino.h>
#include "api_response.h"
#include "config.h"
#ifdef USE_HTTP
  #include <WiFiClient.h>
#else
  #include <WiFiClientSecure.h>
#endif

wl_status_t startWiFi(int &wifiRSSI);
void killWiFi();
bool waitForSNTPSync(tm *timeInfo);
bool printLocalTime(tm *timeInfo);
#ifdef USE_HTTP
  int getOWMonecall(WiFiClient &client, compressed_owm_resp_onecall_t &r);
  int getOWMairpollution(WiFiClient &client, compressed_owm_resp_onecall_t &r);
  int getTranslinkRTTIBusSchedule(WiFiClient &client, compressed_tl_resp_rtti_t &r, const String &stop_num, const String &route_name);
#else
  int getOWMonecall(WiFiClientSecure &client, compressed_owm_resp_onecall_t &r);
  int getOWMairpollution(WiFiClientSecure &client, compressed_owm_resp_onecall_t &r);
  int getTranslinkRTTIBusSchedule(WiFiClientSecure &client, compressed_tl_resp_rtti_t &r, const String &stop_num, const String &route_name);
#endif


#endif

