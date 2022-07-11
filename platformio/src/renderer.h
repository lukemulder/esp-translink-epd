#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <vector>
#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <time.h>
#include "api_response.h"

#define DISP_WIDTH  800
#define DISP_HEIGHT 480

// B/W display
extern GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display;
// 3-colour displays
// extern GxEPD2_3C<GxEPD2_750c, GxEPD2_750c::HEIGHT> display);

typedef enum alignment
{
  LEFT,
  RIGHT,
  CENTER
} alignment_t;

void initDisplay();
void drawString(int x, int y, String text, alignment_t alignment);
void drawCurrentConditions(owm_current_t &current, owm_daily_t &today,
                           owm_resp_air_pollution_t &owm_air_pollution, 
                           float inTemp, float inHumidity);
void drawForecast(owm_daily_t *const daily, tm timeInfo);
void drawAlerts(std::vector<owm_alerts_t> &alerts);
void drawLocationDate(const String &city, tm *timeInfo);
void drawOutlookGraph(owm_hourly_t *const hourly);
void drawStatusBar(char *const statusStr, int wifiRSSI, double batteryVoltage);

void debugDisplayBuffer(owm_resp_onecall_t       &owm_onecall,
                        owm_resp_air_pollution_t &owm_air_pollution);

#endif