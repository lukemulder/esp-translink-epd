# ESP32 E-Paper Weather Display

.

.

.


## Setup Guide

### Components

7.5inch (800×480) E-Ink Display w/ HAT for Raspberry Pi, SPI interface

- Advantages of E-Paper
  - Ultra Low Power Consumption - E-Paper (or E-Ink) displays are ideal for low-power applications that do not require frequent display refreshes. E-Paper displays only draw power when refreshing the display and do not have a backlight. Images will remain on the screen even when power is removed.

- Limitations of E-Paper: 
  - Colors - E-Paper has traditionally been limited to just black and white, but in recent years 3-color E-Paper screens are started to show up.

  - Refresh Times and Ghosting - E-Paper displays are highly susceptible to ghosting effects if refreshed to quickly. To avoid this to  E-Paper displays often take a few seconds to refresh(4s for the unit used in this project) and will alternate between black and white a few times which can be distracting.  


- https://www.waveshare.com/product/7.5inch-e-paper-hat.htm


FireBeetle 2 ESP32-E Microcontroller

- Why the ESP32?

  - Onboard WiFi.

  - 520kB of RAM and 4MB of FLASH. Enough to store lots icons and fonts.

  - Low power consumption.

  - Small size, many small development boards available.

- Why the FireBeetle 2 ESP32-E?

  - Drobot's FireBeetle ESP32 models is optimized for low-power consumption (https://diyi0t.com/reduce-the-esp32-power-consumption/). The Drobot's FireBeetle 2 ESP32-E variant offers USB-C, but older versions of the board with Mirco-USB would work just fine too.

  - Firebeelte ESP32 models include onboard charging circuitry for lithium ion batteries.

  - FireBeetle ESP32 models include onboard circuitry to monitor battery voltage of a battery connected to its JST-PH2.0 connector.


- https://www.dfrobot.com/product-2195.html


BME280 - Pressure, Temperature, and Humidity Sensor


- Provides accurate indoor temperature and humidity.

- Much faster than the DHT22 which requires a 2 second wait before reading temperature and humidity samples.


3.7V Lipo Battery w/ 2 Pin JST Connector 


- Size is up to you. I used a 10000mah battery so that the device can operate on a single charge for 1.5-2 years.


- The battery can be charged by plugging the FireBeetle ESP32 into the wall via the USB-C connector while the battery is plugged into the ESP32's JST connector.


- WARNING: The polarity of JST-PH2.0 connectors is not standardized! You may need to swap order of the wires in the connector.

