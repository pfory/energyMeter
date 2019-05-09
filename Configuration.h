#ifndef CONFIGURATION_H
#define CONFIGURATION_H

//SW name & version
#define     VERSION                       "0.80"
#define     SW_NAME                       "EnergyMeter"

#define timers
//#define ota
#define verbose

#define AUTOCONNECTNAME   "EnergyMeter"
#define AUTOCONNECTPWD    "password"

#ifdef ota
#define HOSTNAMEOTA   "energymeter"
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


#define LEDPIN                BUILTIN_LED //0
#define INTERRUPTPIN          2

#define RTC_ADR               65

char                  mqtt_server[40]       = "192.168.1.56";
uint16_t              mqtt_port             = 1883;
char                  mqtt_username[40]     = "datel";
char                  mqtt_key[20]          = "hanka12";
char                  mqtt_base[60]         = "/home/EnergyMeter/esp02a";
char                  static_ip[16]         = "192.168.1.141";
char                  static_gw[16]         = "192.168.1.1";
char                  static_sn[16]         = "255.255.255.0";

/*
Version history:
--------------------------------------------------------------------------------------------------------------------------
HW
Sonoff
*/
  
#define SENDSTAT_DELAY                       60000 //poslani statistiky kazdou minutu
#endif
