#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <DoubleResetDetector.h>      //https://github.com/khoih-prog/ESP_DoubleResetDetector
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <Ticker.h>
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include "Sender.h"
#include <Wire.h>
#include <PubSubClient.h>

//SW name & version
#define     VERSION                       "0.92"
#define     SW_NAME                       "EnergyMeter"

#define timers
#define ota
#define verbose

#define AUTOCONNECTNAME   "EnergyMeter"
#define AUTOCONNECTPWD    "password"

#ifdef ota
#define HOSTNAMEOTA   SW_NAME VERSION
#endif

#ifdef verbose
  #define DEBUG_PRINT(x)                     Serial.print (x)
  #define DEBUG_PRINTDEC(x)                  Serial.print (x, DEC)
  #define DEBUG_PRINTLN(x)                   Serial.println (x)
  #define DEBUG_PRINTF(x, y)                 Serial.printf (x, y)
  #define PORTSPEED 115200             
  #define DEBUG_WRITE(x)                     Serial.write (x)
  #define DEBUG_PRINTHEX(x)                  Serial.print (x, HEX)
  #define SERIAL_BEGIN                       Serial.begin(PORTSPEED)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTDEC(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x, y)
  #define DEBUG_WRITE(x)
#endif 


#define STATUS_LED                          0 //status LED
#define INTERRUPTPIN                        2

char                          mqtt_server[40]                = "192.168.1.56";
uint16_t                      mqtt_port                      = 1883;
char                          mqtt_username[40]              = "datel";
char                          mqtt_key[20]                   = "hanka12";
char                          mqtt_base[60]                  = "/home/EnergyMeter/esp02";
static const char* const      mqtt_topic_restart             = "restart";
static const char* const      mqtt_topic_netinfo             = "netinfo";

uint32_t              connectDelay                = 30000; //30s
uint32_t              lastConnectAttempt          = 0;  

/*
Version history:
--------------------------------------------------------------------------------------------------------------------------
HW
ESP8266-01
*/

// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 2
// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

#define CONFIG_PORTAL_TIMEOUT 60 //jak dlouho zustane v rezimu AP nez se cip resetuje
#define CONNECT_TIMEOUT 120 //jak dlouho se ceka na spojeni nez se aktivuje config portal
  
#define SENDSTAT_DELAY                       60000 //poslani statistiky kazdou minutu
#define CONNECT_DELAY                        5000 //ms
#endif
