/*ENERGY Meter
--------------------------------------------------------------------------------------------------------------------------
Petr Fory pfory@seznam.cz
GIT - https://github.com/pfory/energyMeter
//ESP8266-01 !!!!!!!SPIFSS!!!!!!!!
*/
#include "Configuration.h"

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <Ticker.h>
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include "Sender.h"
#include <Wire.h>
#include <PubSubClient.h>
#include <FS.h>          

#ifdef ota
#include <ArduinoOTA.h>
#endif

#ifdef serverHTTP
#include <ESP8266WebServer.h>
ESP8266WebServer server(80);
#endif

#ifdef time
#include <TimeLib.h>
#include <Timezone.h>
WiFiUDP EthernetUdp;
static const char     ntpServerName[]       = "tik.cesnet.cz";
//const int timeZone = 2;     // Central European Time
//Central European Time (Frankfurt, Paris)
TimeChangeRule        CEST                  = {"CEST", Last, Sun, Mar, 2, 120};     //Central European Summer Time
TimeChangeRule        CET                   = {"CET", Last, Sun, Oct, 3, 60};       //Central European Standard Time
Timezone CE(CEST, CET);
unsigned int          localPort             = 8888;  // local port to listen for UDP packets
time_t getNtpTime();
#endif


uint32_t      heartBeat                           = 0;

uint32_t      pulseCount                          = 0;
uint32_t      pulseMillisOld                      = 0;
bool          pulseNow                            = false;
uint32_t      pulseLengthMs                       = 0;

bool isDebugEnabled() {
#ifdef verbose
  return true;
#endif // verbose
  return false;
}

//for LED status
#include <Ticker.h>
Ticker ticker;

#ifdef timers
#include <timer.h>
auto timer = timer_create_default(); // create a timer with default settings
Timer<> default_timer; // save as above
#endif

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

  // if (strcmp(topic, "/home/Switch/com")==0) {
    // if (val=="ON") {
      // digitalWrite(RELAYPIN, HIGH);
      // DEBUG_PRINTLN(F("ON"));
    // } else {
      // digitalWrite(RELAYPIN, LOW);
      // DEBUG_PRINTLN(F("OFF"));
    // }
  // }
  
  digitalWrite(LEDPIN, LOW);
  delay(200);
  digitalWrite(LEDPIN, HIGH);

}

WiFiClient espClient;
PubSubClient client(espClient);

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  DEBUG_PRINTLN("Entered config mode");
  DEBUG_PRINTLN(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  DEBUG_PRINTLN(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
}

void validateInput(const char *input, char *output) {
  String tmp = input;
  tmp.trim();
  tmp.replace(' ', '_');
  tmp.toCharArray(output, tmp.length() + 1);
}

ADC_MODE(ADC_VCC); //vcc read

void tick() {
  //toggle state
  int state = digitalRead(LEDPIN);  // get the current state of GPIO1 pin
  digitalWrite(LEDPIN, !state);          // set pin to the opposite state
}


//----------------------------------------------------- S E T U P -----------------------------------------------------------
void setup() {
  // put your setup code here, to run once:
  SERIAL_BEGIN;
  DEBUG_PRINT(F(SW_NAME));
  DEBUG_PRINT(F(" "));
  DEBUG_PRINTLN(F(VERSION));
  
  pinMode(LEDPIN, OUTPUT);

  ticker.attach(1, tick);
    
  WiFi.printDiag(Serial);
    
  // bool validConf = readConfig();
  // if (!validConf) {
    // DEBUG_PRINTLN(F("ERROR config corrupted"));
  // }
  
  rst_info *_reset_info = ESP.getResetInfoPtr();
  uint8_t _reset_reason = _reset_info->reason;
  DEBUG_PRINT("Boot-Mode: ");
  DEBUG_PRINTLN(_reset_reason);
  heartBeat = _reset_reason;

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  // drd.stop();

  // if (_dblreset) {
    // WiFi.disconnect(); //  this alone is not enough to stop the autoconnecter
    // WiFi.mode(WIFI_AP);
  // }
  
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset settings - for testing
  //wifiManager.resetSettings();
  
  IPAddress _ip,_gw,_sn;
  _ip.fromString(static_ip);
  _gw.fromString(static_gw);
  _sn.fromString(static_sn);

  wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);
  
  DEBUG_PRINTLN(_ip);
  DEBUG_PRINTLN(_gw);
  DEBUG_PRINTLN(_sn);

  //wifiManager.setConfigPortalTimeout(60); 
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);
  
    //DEBUG_PRINTLN("Double reset detected. Config mode.");

  WiFiManagerParameter custom_mqtt_server("mqtt_server", "MQTT server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("mqtt_port", "MQTT server port", String(mqtt_port).c_str(), 5);
  WiFiManagerParameter custom_mqtt_uname("mqtt_uname", "MQTT username", mqtt_username, 40);
  WiFiManagerParameter custom_mqtt_key("mqtt_key", "MQTT password", mqtt_key, 20);
  WiFiManagerParameter custom_mqtt_base("mqtt_base", "MQTT topic end without /", mqtt_base, 60);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_uname);
  wifiManager.addParameter(&custom_mqtt_key);
  wifiManager.addParameter(&custom_mqtt_base);

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  
  wifiManager.setTimeout(30);
  wifiManager.setConnectTimeout(30); 
  //wifiManager.setBreakAfterConfig(true);
  
  //set config save notify callback
  //wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(AUTOCONNECTNAME, AUTOCONNECTPWD)) { 
    DEBUG_PRINTLN("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  } 
  
  validateInput(custom_mqtt_server.getValue(), mqtt_server);
  mqtt_port = String(custom_mqtt_port.getValue()).toInt();
  validateInput(custom_mqtt_uname.getValue(), mqtt_username);
  validateInput(custom_mqtt_key.getValue(), mqtt_key);
  validateInput(custom_mqtt_base.getValue(), mqtt_base);
  
  // if (shouldSaveConfig) {
    // saveConfig();
  // }
  
  //if you get here you have connected to the WiFi
  DEBUG_PRINTLN("CONNECTED");
  DEBUG_PRINT("Local ip : ");
  DEBUG_PRINTLN(WiFi.localIP());
  DEBUG_PRINTLN(WiFi.subnetMask());

#ifdef serverHTTP
  server.on ( "/", handleRoot );
  server.begin();
  DEBUG_PRINTLN ( "HTTP server started!!" );
#endif

#ifdef time
  DEBUG_PRINTLN("Setup TIME");
  EthernetUdp.begin(localPort);
  DEBUG_PRINT("Local port: ");
  DEBUG_PRINTLN(EthernetUdp.localPort());
  DEBUG_PRINTLN("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
  
  printSystemTime();
#endif

#ifdef ota
  //OTA
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(HOSTNAMEOTA);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    // String type;
    // if (ArduinoOTA.getCommand() == U_FLASH)
      // type = "sketch";
    // else // U_SPIFFS
      // type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    //DEBUG_PRINTLN("Start updating " + type);
    DEBUG_PRINTLN("Start updating ");
  });
  ArduinoOTA.onEnd([]() {
   DEBUG_PRINTLN("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DEBUG_PRINTF("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    DEBUG_PRINTF("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) DEBUG_PRINTLN("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) DEBUG_PRINTLN("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) DEBUG_PRINTLN("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) DEBUG_PRINTLN("Receive Failed");
    else if (error == OTA_END_ERROR) DEBUG_PRINTLN("End Failed");
  });
  ArduinoOTA.begin();
#endif


  //v klidu 0, kladny pulz po dobu xx ms
  pinMode(INTERRUPTPIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(INTERRUPTPIN), pulseCountEvent, RISING);
  
  // open config file for reading
  if (SPIFFS.exists("/config.ini")) {
    pulseCount = readPulseFromFile();
  } else {
    writePulseToFile(0);
  }
  unsigned long pulseCountMem = readRTCMem();
  Serial.print("Pocet pulzu z RTC pameti:");
  Serial.println(pulseCountMem);
  
  if (pulseCountMem > 0 && pulseCountMem - pulseCount<1000) {
    pulseCount = pulseCountMem;
    Serial.print("Pouziji pocet pulsu z RTM pameti:");
  } else {
    Serial.print("Pouziji pocet pulsu z config.ini");
  }
  Serial.println(pulseCount);
  mqtt.subscribe(&setupPulse);
  mqtt.subscribe(&restart);

#ifdef timers
  //setup timers
  timer.every(SENDSTAT_DELAY, sendStatisticHA);
#endif

  DEBUG_PRINTLN(" Ready");
 
  ticker.detach();
  //keep LED on
  digitalWrite(LEDPIN, HIGH);
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

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
} //loop


bool sendStatisticHA(void *) {
  //printSystemTime();
  digitalWrite(LEDPIN, LOW);
  DEBUG_PRINTLN(F(" - I am sending statistic to HA"));

  SenderClass sender;
  sender.add("VersionSW", VERSION);
  sender.add("Napeti",  ESP.getVcc());
  sender.add("HeartBeat", heartBeat++);
  sender.add("RSSI", WiFi.RSSI());
  
  DEBUG_PRINTLN(F("Calling MQTT"));
  
  sender.sendMQTT(mqtt_server, mqtt_port, mqtt_username, mqtt_key, mqtt_base);
  digitalWrite(LEDPIN, HIGH);
  return true;
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    DEBUG_PRINT("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_base, mqtt_username, mqtt_key)) {
      DEBUG_PRINTLN("connected");
      //client.subscribe("/home/Switch/com");
      client.subscribe((String(mqtt_base) + "/" + "com").c_str());
      DEBUG_PRINT("Substribe topic : ");
      DEBUG_PRINTLN((String(mqtt_base) + "/" + "com").c_str());
    } else {
      DEBUG_PRINT("failed, rc=");
      DEBUG_PRINT(client.state());
      DEBUG_PRINTLN(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void pulseCountEvent() {
  if (millis() - pulseMillisOld>50) {
    pulseCount++;
    pulseLengthMs=millis() - pulseMillisOld;
    pulseMillisOld = millis();
    Serial.println(pulseCount);
    Serial.println(pulseLengthMs);
    pulseNow=true;
  }
}

uint32_t readRTCMem() {
  byte rtcStore[4];
  uint32_t val;
  system_rtc_mem_read(RTC_ADR, rtcStore, 4);
  // Serial.println();
  // Serial.print(rtcStore[0]);
  // Serial.print(":");
  // Serial.print(rtcStore[1]);
  // Serial.print(":");
  // Serial.print(rtcStore[2]);
  // Serial.print(":");
  // Serial.print(rtcStore[3]);
  val = rtcStore[0] | rtcStore[1] << 8 | rtcStore[2] << 16 | rtcStore[3] << 4;
  // Serial.println(val);
  return val;
}

void writeRTCMem(uint32_t val) {
  byte rtcStore[4];
  rtcStore[0] = (val >> 0)  & 0xFFFF;
  rtcStore[1] = (val >> 8)  & 0xFFFF;
  rtcStore[2] = (val >> 16) & 0xFFFF;
  rtcStore[3] = (val >> 24) & 0xFFFF;
  system_rtc_mem_write(RTC_ADR, rtcStore, 4);
}

void writePulseToFile(uint32_t pocet) {
  f = SPIFFS.open("/config.ini", "w");
  if (!f) {
    Serial.println("file open failed");
  } else {
    Serial.print("Zapisuji pocet pulzu ");
    Serial.print(pocet);
    Serial.print(" do souboru config.ini.");
    f.print(pocet);
    f.println();
    f.close();
  }
}

unsigned long readPulseFromFile() {
  f = SPIFFS.open("/config.ini", "r");
  if (!f) {
    Serial.println("file open failed");
    return 0;
  } else {
    Serial.println("====== Reading config from SPIFFS file =======");
    String s = f.readStringUntil('\n');
    Serial.print("Pocet pulzu z config.ini:");
    Serial.println(s);
    f.close();
    return s.toInt();
  }
}