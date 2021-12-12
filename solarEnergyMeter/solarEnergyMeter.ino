/*SOLARENERGY Meter
--------------------------------------------------------------------------------------------------------------------------
Petr Fory pfory@seznam.cz
GIT - https://github.com/pfory/solarEnergyMeter
//Wemos D1
*/
#include "Configuration.h"

#ifdef serverHTTP
#include <ESP8266WebServer.h>
ESP8266WebServer server(80);
#endif

uint32_t      pulseMillisOld1                     = 0;
bool          pulseNow1                           = false;
uint32_t      pulseLengthMs1                      = 0;

uint32_t      pulseMillisOld2                     = 0;
bool          pulseNow2                           = false;
uint32_t      pulseLengthMs2                      = 0;

uint32_t      statMillisOld                       = 0;


void ICACHE_RAM_ATTR pulseCountEvent1() {
  if (millis() - pulseMillisOld1>50) {
    pulseLengthMs1=millis() - pulseMillisOld1;
    pulseMillisOld1 = millis();
    DEBUG_PRINTLN(pulseLengthMs1);
    pulseNow1=true;
  }
}
void ICACHE_RAM_ATTR pulseCountEvent2() {
  if (millis() - pulseMillisOld2>50) {
    pulseLengthMs2=millis() - pulseMillisOld2;
    pulseMillisOld2 = millis();
    DEBUG_PRINTLN(pulseLengthMs2);
    pulseNow2=true;
  }
}

//MQTT callback
void callback(char* topic, byte* payload, unsigned int length) {
  char * pEnd;
  long int valL;
  String val =  String();
  DEBUG_PRINT("Message arrived [");
  DEBUG_PRINT(topic);
  DEBUG_PRINT("] ");
  for (int i=0;i<length;i++) {
    DEBUG_PRINT((char)payload[i]);
    val += (char)payload[i];
  }
  DEBUG_PRINTLN();

  if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_topic_restart)).c_str())==0) {
    DEBUG_PRINT("RESTART");
    ESP.restart();
  } else if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_topic_netinfo)).c_str())==0) {
    sendNetInfoMQTT();
  } else if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_config_portal)).c_str())==0) {
    startConfigPortal();
  } else if (strcmp(topic, (String(mqtt_base) + "/" + String(mqtt_config_portal_stop)).c_str())==0) {
    stopConfigPortal();
  }
}

ADC_MODE(ADC_VCC); //vcc read

//----------------------------------------------------- S E T U P -----------------------------------------------------------
void setup() {
  preSetup();

  //v klidu 0, kladny pulz po dobu xx ms
  pinMode(INTERRUPTPIN1, INPUT);
  attachInterrupt(digitalPinToInterrupt(INTERRUPTPIN1), pulseCountEvent1, RISING);
  pinMode(INTERRUPTPIN2, INPUT);
  attachInterrupt(digitalPinToInterrupt(INTERRUPTPIN2), pulseCountEvent2, RISING);
  
#ifdef timers
  //setup timers
  timer.every(SENDSTAT_DELAY, sendStatisticMQTT);
  timer.every(CONNECT_DELAY, reconnect);
#endif

  void * a;
  reconnect(a);
  sendStatisticMQTT(a);
  sendNetInfoMQTT();
  
  DEBUG_PRINTLN(" Ready");
 
  ticker.detach();
  //keep LED on
  digitalWrite(BUILTIN_LED, HIGH);
  
  drd.stop();

  DEBUG_PRINTLN(F("SETUP END......................."));
} //setup


//----------------------------------------------------- L O O P -----------------------------------------------------------
void loop() {
#ifdef timers
  timer.tick(); // tick the timer
#endif

#ifdef serverHTTP
  server.handleClient();
#endif

#ifdef ota
  ArduinoOTA.handle();
#endif
  wifiManager.process();

  if (pulseNow1) {
    sendDataMQTT1();
    pulseNow1=false;
  }
  if (pulseNow2) {
    sendDataMQTT2();
    pulseNow2=false;
  }
 
  client.loop();
} //loop



void sendDataMQTT1(void) {
  digitalWrite(BUILTIN_LED, LOW);
  DEBUG_PRINTLN(F("Data1"));

  client.publish((String(mqtt_base) + "/pulseLength1").c_str(), String(pulseLengthMs1).c_str());
  
  DEBUG_PRINTLN(F("Calling MQTT"));
  
  digitalWrite(BUILTIN_LED, HIGH);
  return;
}

void sendDataMQTT2(void) {
  digitalWrite(BUILTIN_LED, LOW);
  DEBUG_PRINTLN(F("Data2"));

  client.publish((String(mqtt_base) + "/pulseLength2").c_str(), String(pulseLengthMs2).c_str());
  
  DEBUG_PRINTLN(F("Calling MQTT"));
  
  digitalWrite(BUILTIN_LED, HIGH);
  return;
}

bool reconnect(void *) {
  if (!client.connected()) {
    DEBUG_PRINT("Attempting MQTT connection...");
    if (client.connect(mqtt_base, mqtt_username, mqtt_key, (String(mqtt_base) + "/LWT").c_str(), 2, true, "offline", true)) {
      client.subscribe((String(mqtt_base) + "/" + String(mqtt_topic_restart)).c_str());
      client.subscribe((String(mqtt_base) + "/" + String(mqtt_topic_netinfo)).c_str());
      client.subscribe((String(mqtt_base) + "/" + String(mqtt_config_portal_stop)).c_str());
      client.subscribe((String(mqtt_base) + "/" + String(mqtt_config_portal)).c_str());
      client.publish((String(mqtt_base) + "/LWT").c_str(), "online", true);
      DEBUG_PRINTLN("connected");
    } else {
     DEBUG_PRINT("disconected.");
      DEBUG_PRINT(" Wifi status:");
      DEBUG_PRINT(WiFi.status());
      DEBUG_PRINT(" Client status:");
      DEBUG_PRINTLN(client.state());
     }
  }
  return true;
}