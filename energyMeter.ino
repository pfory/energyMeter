/*ENERGY Meter
--------------------------------------------------------------------------------------------------------------------------
Petr Fory pfory@seznam.cz
GIT - https://github.com/pfory/energyMeter
//ESP8266-01
*/
#include "Configuration.h"

#ifdef serverHTTP
#include <ESP8266WebServer.h>
ESP8266WebServer server(80);
#endif

uint32_t      pulseMillisOld                      = 0;
bool          pulseNow                            = false;
uint32_t      pulseLengthMs                       = 0;

void ICACHE_RAM_ATTR pulseCountEvent() {
  if (millis() - pulseMillisOld>50) {
    pulseLengthMs=millis() - pulseMillisOld;
    pulseMillisOld = millis();
    DEBUG_PRINTLN(pulseLengthMs);
    pulseNow=true;
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
  pinMode(INTERRUPTPIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(INTERRUPTPIN), pulseCountEvent, RISING);
  
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

  if (pulseNow && pulseLengthMs>0) {
    sendDataMQTT();
    pulseNow=false;
  }

  client.loop();
  wifiManager.process();
} //loop

bool sendDataMQTT(void) {
  digitalWrite(BUILTIN_LED, LOW);
  DEBUG_PRINTLN(F("Data"));

  client.publish((String(mqtt_base) + "/pulseLength").c_str(), String(pulseLengthMs).c_str());

  DEBUG_PRINTLN(F("Calling MQTT"));
  
  digitalWrite(BUILTIN_LED, HIGH);
  return true;
}

bool reconnect(void *) {
  if (!client.connected()) {
    DEBUG_PRINT("Attempting MQTT connection...");
    // Attempt to connect
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