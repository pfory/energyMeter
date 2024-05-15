#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

//SW name & version
#define     VERSION                       "0.50"
#define     SW_NAME                       "solarEnergyMeter"

#define timers
#define ota
#define verbose

#define INTERRUPTPIN1                       D1
#define INTERRUPTPIN2                       D2

char                          mqtt_server[40]                = "192.168.1.56";
uint16_t                      mqtt_port                      = 1883;
char                          mqtt_username[40]              = "datel";
char                          mqtt_key[20]                   = "hanka12";
char                          mqtt_base[60]                  = "/home/solarEnergyMeter";
static const char* const      mqtt_topic_restart             = "restart";
static const char* const      mqtt_topic_netinfo             = "netinfo";
static const char* const      mqtt_config_portal             = "config";
static const char* const      mqtt_config_portal_stop        = "disconfig";


/*
Version history:
--------------------------------------------------------------------------------------------------------------------------
HW
ESP8266-01
*/

#define SENDSTAT_DELAY                       60000 //poslani statistiky kazdou minutu
#define CONNECT_DELAY                        5000 //ms

#include <fce.h>

#endif