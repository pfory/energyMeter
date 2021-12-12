#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <Wire.h>
#include <timer.h>
#include <ArduinoJson.h>

//SW name & version
#define     VERSION                       "1.01"
#define     SW_NAME                       "EnergyMeter"

#define timers
#define ota
#define verbose

#define INTERRUPTPIN                        2

char                          mqtt_server[40]                = "192.168.1.56";
uint16_t                      mqtt_port                      = 1883;
char                          mqtt_username[40]              = "datel";
char                          mqtt_key[20]                   = "hanka12";
char                          mqtt_base[60]                  = "/home/EnergyMeter/esp02";
static const char* const      mqtt_topic_restart             = "restart";
static const char* const      mqtt_topic_netinfo             = "netinfo";
static const char* const      mqtt_config_portal             = "config";
static const char* const      mqtt_config_portal_stop        = "disconfig";

uint32_t              connectDelay                = 30000; //30s
uint32_t              lastConnectAttempt          = 0;  

/*
Version history:
--------------------------------------------------------------------------------------------------------------------------
HW
ESP8266-01
*/

#define CONFIG_PORTAL_TIMEOUT 60 //jak dlouho zustane v rezimu AP nez se cip resetuje
#define CONNECT_TIMEOUT 120 //jak dlouho se ceka na spojeni nez se aktivuje config portal
  
#define SENDSTAT_DELAY                       60000 //poslani statistiky kazdou minutu
#define CONNECT_DELAY                        5000 //ms

#include <fce.h>

#endif
