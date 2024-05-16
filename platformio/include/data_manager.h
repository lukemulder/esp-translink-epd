/* Data Manager for esp32-translink-epd.
 * Copyright (C) 2022-2024  Luke Mulder
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
#ifndef __DATA_MANAGER_H__
#define __DATA_MANAGER_H__

#include <Arduino.h>
#include <time.h>
#include <WiFi.h>
#include <Wire.h>
#include "config.h"
#include "api_response.h"
#include "display_utils.h"
#include "client_utils.h"
#include "renderer.h"

#if defined(USE_HTTPS_WITH_CERT_VERIF) || defined(USE_HTTPS_WITH_CERT_VERIF)
  #include <WiFiClientSecure.h>
#endif
#ifdef USE_HTTPS_WITH_CERT_VERIF
  #include "cert.h"
#endif

extern RTC_DATA_ATTR compressed_owm_resp_onecall_t  comp_owm_onecall;
extern RTC_DATA_ATTR compressed_tl_resp_rtti_t      comp_translink_rtti_schedules[TRANSLINK_BUSES_DISPLAYED];

class DataManager
{
    public:
        void init(tm* currentTime, bool firstBoot);
        int updateTranslinkData();
        int updateOpenWeatherMapData();

        bool translinkDataStale();
        bool openWeatherMapDataStale();

        compressed_tl_resp_rtti_t* getTranslinkData();
        compressed_owm_resp_onecall_t* getOpenWeatherMapData();

        void setCurrentTime(tm* currentTime);

    private:
        void evalTranslinkDataStale();
        void evalOpenWeatherMapDataStale();

        void processTranslinkSchedules();
        void addDateStampTranslinkSchedule(compressed_tl_resp_rtti_t &s);
        tm busScheduleEntryTo24TM(char s[MAX_EXPECTED_LEAVE_STR_SIZE], int yday);

        bool tlDataStale;
        bool OWMDataStale;

        tm currentTime;

        compressed_owm_resp_onecall_t *owm_onecall;
        compressed_tl_resp_rtti_t     *translink_rtti_schedules;

        #ifdef USE_HTTP
        WiFiClient client;
        #elif defined(USE_HTTPS_NO_CERT_VERIF)
        WiFiClientSecure client;
        #elif defined(USE_HTTPS_WITH_CERT_VERIF)
        WiFiClientSecure client;
        #endif
};

#endif