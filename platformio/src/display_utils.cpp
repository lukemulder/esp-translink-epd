#include <vector>
#include <Arduino.h>

#include "api_response.h"
#include "config.h"
#include "display_utils.h"

// icon header files
#include "icons/icons_32x32.h"
#include "icons/icons_48x48.h"
#include "icons/icons_64x64.h"
#include "icons/icons_196x196.h"

/* Takes a String and capitalizes the first letter of every word.
 *
 * Ex:
 *   input   : "severe thunderstorm warning" or "SEVERE THUNDERSTORM WARNING"
 *   becomes : "Severe Thunderstorm Warning"
 */
void toTitleCase(String &text)
{
  text.setCharAt(0, toUpperCase(text.charAt(0)));

  for (int i = 1; i < text.length(); ++i)
  {
    if (text.charAt(i - 1) == ' ' 
     || text.charAt(i - 1) == '-' 
     || text.charAt(i - 1) == '(')
    {
      text.setCharAt(i, toUpperCase(text.charAt(i)));
    }
    else
    {
      text.setCharAt(i, toLowerCase(text.charAt(i)));
    }
  }

  return;
}

/* Takes a String and truncates at any of these characters ,.( and trims any
 * trailing whitespace.
 *
 * Ex:
 *   input   : "Severe Thunderstorm Warning, (Starting At 10 Pm)"
 *   becomes : "Severe Thunderstorm Warning"
 */
void truncateExtraneousInfo(String &text)
{
  if (text.isEmpty())
  {
    return;
  }

  int i = 1;
  int lastChar = i;
  while (i < text.length() 
   && (text.charAt(i) != ',' 
    || text.charAt(i) != '.' 
    || text.charAt(i) != '('))
  {
    if (text.charAt(i) != ' ')
    {
      lastChar = i + 1;
    }
    ++i;
  }

  text = text.substring(0, lastChar);

  return;
}

/* Returns the urgency of an event based by checking if the event String
 * contains any indicator keywords.
 *
 * Urgency keywords are defined in config.h because they are very regional.
 *   ex: United States - (Watch < Advisory < Warning)
 *
 * The index in vector<String> ALERT_URGENCY indicates the urgency level.
 * If an event string matches none of these keywords the urgency is unknown, -1
 * is returned.
 * In the United States example, Watch = 0, Advisory = 1, Warning = 2
 */
int eventUrgency(String &event)
{
  int urgency_lvl = -1;
  for (int i = 0; i < ALERT_URGENCY.size(); ++i)
  {
    if (event.indexOf(ALERT_URGENCY[i]) > 0)
    {
      urgency_lvl = i;
    }
  }
  return urgency_lvl;
}

/* This algorithm prepares alerts from the API responses to be displayed.
 *
 * Background:
 * The display layout is setup to show up to 2 alerts, but alerts can be 
 * unpredictible in severity and number. If more than 2 alerts are active, this 
 * algorithm will attempt to interpret the urgency of each alert and prefer to 
 * display the most urgent and recently issued alerts of each event type. 
 * Depending on the region different keywords are used to convey the level of 
 * urgency.
 *
 * A vector array is used to store these keywords. (defined in config.h) Urgency
 * is ranked from low to high where the first index of the vector is the least
 * urgent keyword and the last index is the most urgent keyword. Expected as all
 * lowercase.
 *
 *
 * Pseudo Code:
 * Convert all event text and tags to lowercase.
 *
 * // Deduplicate alerts of the same type
 * Dedup alerts with the same first tag. (ie. tag 0) Keeping only the most
 *   urgent alerts of each tag and alerts who's urgency cannot be determined.
 * Note: urgency keywords are defined in config.h because they are very
 *       regional. ex: United States - (Watch < Advisory < Warning)
 *
 * // Save only the 2 most recent alerts
 * If (more than 2 weather alerts remain)
 *   Keep only the 2 most recently issued alerts (aka greatest "start" time)
 *   OpenWeatherMap provides this order, so we can just take index 0 and 1.
 *
 * Truncate Extraneous Info (anything that follows a comma, period, or open
 *   parentheses)
 * Convert all event text to Title Case. (Every Word Has Capital First Letter)
 */
void prepareAlerts(std::vector<owm_alerts_t> &resp)
{
  // Convert all event text and tags to lowercase.
  for (auto alert : resp)
  {
    alert.event.toLowerCase();
    alert.tags.toLowerCase();
  }

  // Deduplicate alerts with the same first tag. Keeping only the most urgent
  // alerts of each tag and alerts who's urgency cannot be determined.
  for (auto it_a = resp.begin(); it_a != resp.end(); ++it_a)
  {
    if (it_a->tags.isEmpty())
    {
      continue; // urgency can not be determined so it remains in the list
    }

    for (auto it_b = resp.begin(); it_b != resp.end(); ++it_b)
    {
      if (it_a != it_b && it_a->tags == it_b->tags)
      {
        // comparing alerts of the same tag, removing the less urgent alert
        if (eventUrgency(it_a->event) >= eventUrgency(it_b->event))
        {
          resp.erase(it_b);
        }
      }
    }
  }

  // Save only the 2 most recent alerts
  while (resp.size() > 2)
  {
    resp.pop_back();
  }

  // Prettify event strings
  for (auto alert : resp)
  {
    truncateExtraneousInfo(alert.event);
    toTitleCase(alert.event);
  }

  return;
}

/* Takes the daily weather forecast (from OpenWeatherMap API 
 * response) and returns a pointer to the icon's 64x64 bitmap.
 *
 * Uses multiple factors to return more detailed icons than the simple icon 
 * catagories that OpenWeatherMap provides.
 * 
 * Last Updated: June 26, 2022
 * 
 * References: 
 *   https://openweathermap.org/weather-conditions
 *   https://www.weather.gov/ajk/ForecastTerms
 */
const uint8_t *getForecastBitmap(owm_daily_t &daily)
{
  int id = daily.weather.id;
  // always using the day icon for weather forecast
  // bool day = daily.weather.icon.charAt(daily.weather.icon.length() - 1) == 'd';
  bool cloudy = daily.clouds > 60.25; // partly cloudy / partly sunny
  bool windy = (UNITS == 'i' 
                      && (daily.wind_speed >= 20   || daily.wind_gust >= 25))
            || (UNITS == 'm' 
                      && (daily.wind_speed >= 32.2 || daily.wind_gust >= 40.2));

  switch (id)
  {
  // Group 2xx: Thunderstorm
  case 200: // Thunderstorm  thunderstorm with light rain     11d
  case 201: // Thunderstorm  thunderstorm with rain           11d
  case 202: // Thunderstorm  thunderstorm with heavy rain     11d
  case 210: // Thunderstorm  light thunderstorm               11d
  case 211: // Thunderstorm  thunderstorm                     11d
  case 212: // Thunderstorm  heavy thunderstorm               11d
  case 221: // Thunderstorm  ragged thunderstorm              11d
    if (!cloudy) {return wi_day_thunderstorm_64x64;}
    return wi_thunderstorm_64x64;
  case 230: // Thunderstorm  thunderstorm with light drizzle  11d
  case 231: // Thunderstorm  thunderstorm with drizzle        11d
  case 232: // Thunderstorm  thunderstorm with heavy drizzle  11d
    if (!cloudy) {return wi_day_storm_showers_64x64;}
    return wi_storm_showers_64x64;
  // Group 3xx: Drizzle
  case 300: // Drizzle       light intensity drizzle          09d
  case 301: // Drizzle       drizzle                          09d
  case 302: // Drizzle       heavy intensity drizzle          09d
  case 310: // Drizzle       light intensity drizzle rain     09d
  case 311: // Drizzle       drizzle rain                     09d
  case 312: // Drizzle       heavy intensity drizzle rain     09d
  case 313: // Drizzle       shower rain and drizzle          09d
  case 314: // Drizzle       heavy shower rain and drizzle    09d
  case 321: // Drizzle       shower drizzle                   09d
    if (!cloudy) {return wi_day_showers_64x64;}
    return wi_showers_64x64;
  // Group 5xx: Rain
  case 500: // Rain          light rain                       10d
  case 501: // Rain          moderate rain                    10d
  case 502: // Rain          heavy intensity rain             10d
  case 503: // Rain          very heavy rain                  10d
  case 504: // Rain          extreme rain                     10d
    if (!cloudy && windy) {return wi_day_rain_wind_64x64;}
    if (!cloudy)          {return wi_day_rain_64x64;}
    if (windy)            {return wi_rain_wind_64x64;}
    return wi_rain_64x64;
  case 511: // Rain          freezing rain                    13d
    if (!cloudy) {return wi_day_rain_mix_64x64;}
    return wi_rain_mix_64x64;
  case 520: // Rain          light intensity shower rain      09d
  case 521: // Rain          shower rain                      09d
  case 522: // Rain          heavy intensity shower rain      09d
  case 531: // Rain          ragged shower rain               09d
    if (!cloudy) {return wi_day_showers_64x64;}
    return wi_showers_64x64;
  // Group 6xx: Snow
  case 600: // Snow          light snow                       13d
  case 601: // Snow          Snow                             13d
  case 602: // Snow          Heavy snow                       13d
    if (!cloudy && windy) {return wi_day_snow_wind_64x64;}
    if (!cloudy)          {return wi_day_snow_64x64;}
    if (windy)            {return wi_snow_wind_64x64;}
    return wi_snow_64x64;
  case 611: // Snow          Sleet                            13d
  case 612: // Snow          Light shower sleet               13d
  case 613: // Snow          Shower sleet                     13d
    if (!cloudy) {return wi_day_sleet_64x64;}
    return wi_sleet_64x64;
  case 615: // Snow          Light rain and snow              13d
  case 616: // Snow          Rain and snow                    13d
  case 620: // Snow          Light shower snow                13d
  case 621: // Snow          Shower snow                      13d
  case 622: // Snow          Heavy shower snow                13d
    if (!cloudy) {return wi_day_rain_mix_64x64;}
    return wi_rain_mix_64x64;
  // Group 7xx: Atmosphere
  case 701: // Mist          mist                             50d
    if (!cloudy) {return wi_day_fog_64x64;}
    return wi_fog_64x64;
  case 711: // Smoke         Smoke                            50d
    return wi_smoke_64x64;
  case 721: // Haze          Haze                             50d
    return wi_day_haze_64x64;
  case 731: // Dust          sand/dust whirls                 50d
    return wi_sandstorm_64x64;
  case 741: // Fog           fog                              50d
    if (!cloudy) {return wi_day_fog_64x64;}
    return wi_fog_64x64;
  case 751: // Sand          sand                             50d
    return wi_sandstorm_64x64;
  case 761: // Dust          dust                             50d
    return wi_dust_64x64;
  case 762: // Ash           volcanic ash                     50d
    return wi_volcano_64x64;
  case 771: // Squall        squalls                          50d
    return wi_cloudy_gusts_64x64;
  case 781: // Tornado       tornado                          50d
    return wi_tornado_64x64;
  // Group 800: Clear
  case 800: // Clear         clear sky                        01d 01n
    if (windy) {return wi_windy_64x64;}
    return wi_day_sunny_64x64;
  // Group 80x: Clouds
  case 801: // Clouds        few clouds: 11-25%               02d 02n
    if (windy) {return wi_day_cloudy_windy_64x64;}
    return wi_day_sunny_overcast_64x64;
  case 802: // Clouds        scattered clouds: 25-50%         03d 03n
  case 803: // Clouds        broken clouds: 51-84%            04d 04n
    if (windy) {return wi_day_cloudy_windy_64x64;}
    return wi_day_cloudy_64x64;
  case 804: // Clouds        overcast clouds: 85-100%         04d 04n
    if (windy) {return wi_cloudy_windy_64x64;}
    return wi_cloudy_64x64;
  default:
    // OpenWeatherMap maybe this is a new icon in one of the existing groups
    if (id >= 200 && id < 300) {return wi_thunderstorm_64x64;}
    if (id >= 300 && id < 400) {return wi_showers_64x64;}
    if (id >= 500 && id < 600) {return wi_rain_64x64;}
    if (id >= 600 && id < 700) {return wi_snow_64x64;}
    if (id >= 700 && id < 800) {return wi_fog_64x64;}
    if (id >= 800 && id < 900) {return wi_cloudy_64x64;}
    return wi_na_64x64;
  }
}

/* Takes the current weather (from OpenWeatherMap API response) and returns a 
 * pointer to the icon's 196x196 bitmap.
 *
 * Uses multiple factors to return more detailed icons than the simple icon 
 * catagories that OpenWeatherMap provides.
 * 
 * Last Updated: June 26, 2022
 * 
 * References: 
 *   https://openweathermap.org/weather-conditions
 *   https://www.weather.gov/ajk/ForecastTerms
 */
const uint8_t *getCurrentBitmap(owm_current_t &current)
{
  int id = current.weather.id;
  bool day = current.weather.icon.charAt(
                                      current.weather.icon.length() - 1) == 'd';
  bool cloudy = current.clouds > 60.25; // partly cloudy / partly sunny
  bool windy = (UNITS == 'i' 
                  && (current.wind_speed >= 20   || current.wind_gust >= 25))
            || (UNITS == 'm' 
                  && (current.wind_speed >= 32.2 || current.wind_gust >= 40.2));

  switch (id)
  {
  // Group 2xx: Thunderstorm
  case 200: // Thunderstorm  thunderstorm with light rain     11d
  case 201: // Thunderstorm  thunderstorm with rain           11d
  case 202: // Thunderstorm  thunderstorm with heavy rain     11d
  case 210: // Thunderstorm  light thunderstorm               11d
  case 211: // Thunderstorm  thunderstorm                     11d
  case 212: // Thunderstorm  heavy thunderstorm               11d
  case 221: // Thunderstorm  ragged thunderstorm              11d
    if (!cloudy && day)  {return wi_day_thunderstorm_196x196;}
    if (!cloudy && !day) {return wi_night_alt_thunderstorm_196x196;}
    return wi_thunderstorm_196x196;
  case 230: // Thunderstorm  thunderstorm with light drizzle  11d
  case 231: // Thunderstorm  thunderstorm with drizzle        11d
  case 232: // Thunderstorm  thunderstorm with heavy drizzle  11d
    if (!cloudy && day)  {return wi_day_storm_showers_196x196;}
    if (!cloudy && !day) {return wi_night_alt_storm_showers_196x196;}
    return wi_storm_showers_196x196;
  // Group 3xx: Drizzle
  case 300: // Drizzle       light intensity drizzle          09d
  case 301: // Drizzle       drizzle                          09d
  case 302: // Drizzle       heavy intensity drizzle          09d
  case 310: // Drizzle       light intensity drizzle rain     09d
  case 311: // Drizzle       drizzle rain                     09d
  case 312: // Drizzle       heavy intensity drizzle rain     09d
  case 313: // Drizzle       shower rain and drizzle          09d
  case 314: // Drizzle       heavy shower rain and drizzle    09d
  case 321: // Drizzle       shower drizzle                   09d
    if (!cloudy && day)  {return wi_day_showers_196x196;}
    if (!cloudy && !day) {return wi_night_alt_showers_196x196;}
    return wi_showers_196x196;
  // Group 5xx: Rain
  case 500: // Rain          light rain                       10d
  case 501: // Rain          moderate rain                    10d
  case 502: // Rain          heavy intensity rain             10d
  case 503: // Rain          very heavy rain                  10d
  case 504: // Rain          extreme rain                     10d
    if (!cloudy && day && windy)  {return wi_day_rain_wind_196x196;}
    if (!cloudy && day)           {return wi_day_rain_196x196;}
    if (!cloudy && !day && windy) {return wi_night_alt_rain_wind_196x196;}
    if (!cloudy && !day)          {return wi_night_alt_rain_196x196;}
    if (windy)                    {return wi_rain_wind_196x196;}
    return wi_rain_196x196;
  case 511: // Rain          freezing rain                    13d
    if (!cloudy && day)  {return wi_day_rain_mix_196x196;}
    if (!cloudy && !day) {return wi_night_alt_rain_mix_196x196;}
    return wi_rain_mix_196x196;
  case 520: // Rain          light intensity shower rain      09d
  case 521: // Rain          shower rain                      09d
  case 522: // Rain          heavy intensity shower rain      09d
  case 531: // Rain          ragged shower rain               09d
    if (!cloudy && day)  {return wi_day_showers_196x196;}
    if (!cloudy && !day) {return wi_night_alt_showers_196x196;}
    return wi_showers_196x196;
  // Group 6xx: Snow
  case 600: // Snow          light snow                       13d
  case 601: // Snow          Snow                             13d
  case 602: // Snow          Heavy snow                       13d
    if (!cloudy && day && windy)  {return wi_day_snow_wind_196x196;}
    if (!cloudy && day)           {return wi_day_snow_196x196;}
    if (!cloudy && !day && windy) {return wi_night_alt_snow_wind_196x196;}
    if (!cloudy && !day)          {return wi_night_alt_snow_196x196;}
    if (windy)                    {return wi_snow_wind_196x196;}
    return wi_snow_196x196;
  case 611: // Snow          Sleet                            13d
  case 612: // Snow          Light shower sleet               13d
  case 613: // Snow          Shower sleet                     13d
    if (!cloudy && day)  {return wi_day_sleet_196x196;}
    if (!cloudy && !day) {return wi_night_alt_sleet_196x196;}
    return wi_sleet_196x196;
  case 615: // Snow          Light rain and snow              13d
  case 616: // Snow          Rain and snow                    13d
  case 620: // Snow          Light shower snow                13d
  case 621: // Snow          Shower snow                      13d
  case 622: // Snow          Heavy shower snow                13d
    if (!cloudy && day)  {return wi_day_rain_mix_196x196;}
    if (!cloudy && !day) {return wi_night_alt_rain_mix_196x196;}
    return wi_rain_mix_196x196;
  // Group 7xx: Atmosphere
  case 701: // Mist          mist                             50d
    if (!cloudy && day)  {return wi_day_fog_196x196;}
    if (!cloudy && !day) {return wi_night_fog_196x196;}
    return wi_fog_196x196;
  case 711: // Smoke         Smoke                            50d
    return wi_smoke_196x196;
  case 721: // Haze          Haze                             50d
    return wi_day_haze_196x196;
  case 731: // Dust          sand/dust whirls                 50d
    return wi_sandstorm_196x196;
  case 741: // Fog           fog                              50d
    if (!cloudy && day)  {return wi_day_fog_196x196;}
    if (!cloudy && !day) {return wi_night_fog_196x196;}
    return wi_fog_196x196;
  case 751: // Sand          sand                             50d
    return wi_sandstorm_196x196;
  case 761: // Dust          dust                             50d
    return wi_dust_196x196;
  case 762: // Ash           volcanic ash                     50d
    return wi_volcano_196x196;
  case 771: // Squall        squalls                          50d
    return wi_cloudy_gusts_196x196;
  case 781: // Tornado       tornado                          50d
    return wi_tornado_196x196;
  // Group 800: Clear
  case 800: // Clear         clear sky                        01d 01n
    if (windy) {return wi_windy_196x196;}
    if (!day)  {return wi_night_clear_196x196;}
    return wi_day_sunny_196x196;
  // Group 80x: Clouds
  case 801: // Clouds        few clouds: 11-25%               02d 02n
    if (windy && day)  {return wi_day_cloudy_windy_196x196;}
    if (windy && !day) {return wi_night_alt_cloudy_windy_196x196;}
    if (!day)          {return wi_night_alt_partly_cloudy_196x196;}
    return wi_day_sunny_overcast_196x196;
  case 802: // Clouds        scattered clouds: 25-50%         03d 03n
  case 803: // Clouds        broken clouds: 51-84%            04d 04n
    if (windy && day)  {return wi_day_cloudy_windy_196x196;}
    if (windy && !day) {return wi_night_alt_cloudy_windy_196x196;}
    if (!day)          {return wi_night_alt_cloudy_196x196;}
    return wi_day_cloudy_196x196;
  case 804: // Clouds        overcast clouds: 85-100%         04d 04n
    if (windy) {return wi_cloudy_windy_196x196;}
    return wi_cloudy_196x196;
  default:
    // OpenWeatherMap maybe this is a new icon in one of the existing groups
    if (id >= 200 && id < 300) {return wi_thunderstorm_196x196;}
    if (id >= 300 && id < 400) {return wi_showers_196x196;}
    if (id >= 500 && id < 600) {return wi_rain_196x196;}
    if (id >= 600 && id < 700) {return wi_snow_196x196;}
    if (id >= 700 && id < 800) {return wi_fog_196x196;}
    if (id >= 800 && id < 900) {return wi_cloudy_196x196;}
    return wi_na_196x196;
  }
}



const uint8_t *getAlertBitmap48(owm_alerts_t &alert)
{
  



}


const uint8_t *getAlertBitmap32(owm_alerts_t &alert);