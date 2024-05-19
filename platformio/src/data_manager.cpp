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

#include "data_manager.h"

#define VALID_YEAR 2000

RTC_DATA_ATTR compressed_owm_resp_onecall_t  comp_owm_onecall = {0};
RTC_DATA_ATTR compressed_tl_resp_rtti_t      comp_translink_rtti_schedules[TRANSLINK_BUSES_DISPLAYED] = {0};

void DataManager::init(tm* currentTime, bool firstBoot)
{
  // MAKE API REQUESTS
#ifdef USE_HTTP

#elif defined(USE_HTTPS_NO_CERT_VERIF)
  this->client.setInsecure();
#elif defined(USE_HTTPS_WITH_CERT_VERIF)
  this->client.setCACert(cert_Sectigo_RSA_Domain_Validation_Secure_Server_CA);
#endif

    Serial.println("Data Manager time: " + String(currentTime->tm_hour) + ":" + String(currentTime->tm_min));

    setCurrentTime(currentTime);

    owm_onecall = &comp_owm_onecall;
    translink_rtti_schedules = comp_translink_rtti_schedules;

    if (firstBoot)
    {
        Serial.println("Data Manager: first boot");
        tlDataStale = true;
        OWMDataStale = true;

        for(int i = 0; i < TRANSLINK_BUSES_DISPLAYED; i++)
        {
            comp_translink_rtti_schedules[i].valid_schedules = 0;
        }
    }
    else
    {
        processTranslinkSchedules();

        evalOpenWeatherMapDataStale();
        evalTranslinkDataStale();
    }
}

void DataManager::setCurrentTime(tm* currentTime)
{
    if (currentTime != nullptr) {
        memcpy(&this->currentTime, currentTime, sizeof(tm));
    }
}

int isLeapYear(int year) {
    // Adjust year since tm_year is the number of years since 1900
    year += 1900;

    // Leap year check
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        return 1;
    } else {
        return 0;
    }
}

void DataManager::addDateStampTranslinkSchedule(compressed_tl_resp_rtti_t &s)
{
  int numberDaysInDay = (isLeapYear(currentTime.tm_year)) ? 366 - 1 : 365 - 1;

  for(int i = 0; i < RTTI_NUM_SCHEDULES; i++)
  {
    tm arrivalTime = busScheduleEntryTo24TM(s.schedules[i].expected_leave_time, 0);

    // If arrival time has rolled over to next day
    if(currentTime.tm_hour - arrivalTime.tm_hour > 12)
    {
      s.schedules[i].tm_yday = (currentTime.tm_yday + 1) % numberDaysInDay;
    }
    else
    {
      s.schedules[i].tm_yday = currentTime.tm_yday;
    }
  }
}

int DataManager::updateTranslinkData()
{
  String statusStr = {};
  String tmpStr = {};

  int rxStatus = HTTP_CODE_OK;
  // TRANSLINK API CALL
  for(int i = 0; i < TRANSLINK_BUSES_DISPLAYED; i++)
  {
    if(translink_rtti_schedules[i].valid_schedules < 3)
    {
        rxStatus = getTranslinkRTTIBusSchedule(client, translink_rtti_schedules[i], translink_bus_info[i].stop_num, translink_bus_info[i].route_name);
        if (rxStatus != HTTP_CODE_OK)
        {
        return rxStatus;
        }
        addDateStampTranslinkSchedule(translink_rtti_schedules[i]);
    }
  }

  processTranslinkSchedules();

  return rxStatus;
}

int DataManager::updateOpenWeatherMapData()
{
  int rxStatus = HTTP_CODE_OK;

    // WEATHER ONE CALL
  if(OWMDataStale)
  {
    rxStatus = getOWMonecall(client, *owm_onecall);
  }

  return rxStatus;
}

compressed_tl_resp_rtti_t* DataManager::getTranslinkData()
{
    return translink_rtti_schedules;
}

compressed_owm_resp_onecall_t* DataManager::getOpenWeatherMapData()
{
    return owm_onecall;
}

void DataManager::evalTranslinkDataStale()
{
  // Translink data must be refreshed at this interval to ensure
  // schedule doesn't drift over time
  if(((currentTime.tm_min % DATA_RESET_TIME_TRANSLINK) * 60ULL
                    + currentTime.tm_sec) < (SLEEP_DURATION_TRANSLINK * 60ULL))
  {
    for(int i = 0; i < TRANSLINK_BUSES_DISPLAYED; i++)
    {
      comp_translink_rtti_schedules[i].valid_schedules = 0;
    }
    
    tlDataStale = true;
    return;
  }

  // If not enough schedules to display then data stale
  for(int i = 0; i < TRANSLINK_BUSES_DISPLAYED; i++)
  {
    if(comp_translink_rtti_schedules[i].valid_schedules < 3)
    {
      tlDataStale = true;
      return;
    }
  }

  tlDataStale = false;
}

void DataManager::evalOpenWeatherMapDataStale()
{
  // If no data in struct on wake up then data stale
  if(comp_owm_onecall.current_dt == 0 || comp_owm_onecall.current_conditions_bitmap_196 == nullptr)
  {
    OWMDataStale = true;
    return;
  }

  // If woken by Translink Call time then check to see if also aligns with OpenWeatherCall
  // Call are driven by the minutes of the hour not an interval
  if(SLEEP_DURATION_OWM > SLEEP_DURATION_TRANSLINK)
  {
    if(((currentTime.tm_min % SLEEP_DURATION_OWM) * 60ULL
                      + currentTime.tm_sec) < (SLEEP_DURATION_TRANSLINK * 60ULL))
    {
      Serial.println("Time To Poll: Within threshold for OpenWeatherMap polling");
      OWMDataStale = true;
      return;
    }
    else
    {
      OWMDataStale = false;
      return;
    }
  }

  OWMDataStale = true;
}

bool DataManager::translinkDataStale()
{
    return tlDataStale;
}

bool DataManager::openWeatherMapDataStale()
{
    return OWMDataStale;
}

tm DataManager::busScheduleEntryTo24TM(char s[MAX_EXPECTED_LEAVE_STR_SIZE], int yday)
{
  int hours;
  int mins;
  char period[3];

  tm time = {0};

  if(sscanf(s, "%d:%d%2s", &hours, &mins, period) == 3)
  {
    // if pm then convert hours to 24 hours
    if(period[0] == 'p' && hours < 12)
    {
      hours += 12;
    }

    time.tm_hour = hours;
    time.tm_min = mins;
    time.tm_yday = yday;
  
    time.tm_year = VALID_YEAR;
  }

  return time;
}

bool busScheduleEntryExpired(tm &arrivalTime, tm &currentTime, int busRunTime)
{
  bool validSchedule = (arrivalTime.tm_yday == (currentTime.tm_yday + 1 % 365) ||
                       (arrivalTime.tm_yday == currentTime.tm_yday && arrivalTime.tm_hour > currentTime.tm_hour) ||
                       (arrivalTime.tm_yday == currentTime.tm_yday && arrivalTime.tm_hour == currentTime.tm_hour && (arrivalTime.tm_min > currentTime.tm_min + busRunTime + 1)));
  return !validSchedule;
}

int busScheduleCountdownMin(tm& arrivalTime, tm& currentTime)
{
  int mins;

  if(arrivalTime.tm_yday != currentTime.tm_yday)
  {
    mins = ((24 - currentTime.tm_hour + arrivalTime.tm_hour) * 60) + 
           ((60 - currentTime.tm_min) + arrivalTime.tm_hour - 1);
  }
  else
  {
    mins = ((arrivalTime.tm_hour - currentTime.tm_hour) * 60) + 
           (arrivalTime.tm_min - currentTime.tm_min - 1);
  }

  return mins;
}

bool isTimeValid(tm &arrivalTime)
{
  return (arrivalTime.tm_year == VALID_YEAR);
}

void DataManager::processTranslinkSchedules()
{
  tm arrivalTime;

  for(int bus = 0; bus < TRANSLINK_BUSES_DISPLAYED; bus++)
  {
    int staleSchedules = 0;

    for(int schedule = 0; schedule < RTTI_NUM_SCHEDULES && schedule < comp_translink_rtti_schedules[bus].valid_schedules; schedule++)
    {
      arrivalTime = busScheduleEntryTo24TM(comp_translink_rtti_schedules[bus].schedules[schedule].expected_leave_time, comp_translink_rtti_schedules[bus].schedules[schedule].tm_yday);

      Serial.println(comp_translink_rtti_schedules[bus].schedules[schedule].expected_leave_time);

      if(isTimeValid(arrivalTime))
      {
        // Remove elements from schedule that are expired
        if(busScheduleEntryExpired(arrivalTime, currentTime, translink_bus_info[bus].run_time))
        {
          staleSchedules++;
        }
        // Update the expected countdown to reflect the new time
        else
        {
          comp_translink_rtti_schedules[bus].schedules[schedule].expected_countdown = busScheduleCountdownMin(arrivalTime, currentTime);
        }
      }
      else // failed to parse
      {
        staleSchedules++;
      }
    }

    // Remove stale schedules from struct
    if(staleSchedules > 0)
    {
      comp_translink_rtti_schedules[bus].valid_schedules -= staleSchedules;
      memmove(&comp_translink_rtti_schedules[bus].schedules[0], &comp_translink_rtti_schedules[bus].schedules[staleSchedules], sizeof(compressed_rtti_schedule_t) * (comp_translink_rtti_schedules[bus].valid_schedules));
    }
  }
}