/*ENERGY Meter
--------------------------------------------------------------------------------------------------------------------------
Petr Fory pfory@seznam.cz
GIT - https://github.com/pfory/solarEnergyMeter
//Wemos D1
*/
#include "Configuration.h"

#ifdef ota
#include <ArduinoOTA.h>
#endif

#ifdef serverHTTP
#include <ESP8266WebServer.h>
ESP8266WebServer server(80);
#endif

DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

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

uint32_t      pulseMillisOld1                     = 0;
bool          pulseNow1                           = false;
uint32_t      pulseLengthMs1                      = 0;

uint32_t      pulseMillisOld2                     = 0;
bool          pulseNow2                           = false;
uint32_t      pulseLengthMs2                      = 0;

uint32_t      statMillisOld                       = 0;

WiFiClient espClient;
PubSubClient client(espClient);


bool isDebugEnabled() {
#ifdef verbose
  return true;
#endif // verbose
  return false;
}

//for LED status
#include <Ticker.h>
Ticker ticker;

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

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  DEBUG_PRINTLN("Entered config mode");
  DEBUG_PRINTLN(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  DEBUG_PRINTLN(myWiFiManager->getConfigPortalSSID());
}

ADC_MODE(ADC_VCC); //vcc read

void tick() {
  //toggle state
  int state = digitalRead(STATUS_LED);  // get the current state of GPIO1 pin
  digitalWrite(STATUS_LED, !state);          // set pin to the opposite state
}

WiFiManager wifiManager;

void ICACHE_RAM_ATTR handleInterrupt();

//----------------------------------------------------- S E T U P -----------------------------------------------------------
void setup() {
  // put your setup code here, to run once:
  SERIAL_BEGIN;
  DEBUG_PRINT(F(SW_NAME));
  DEBUG_PRINT(F(" "));
  DEBUG_PRINTLN(F(VERSION));
  
  pinMode(STATUS_LED, OUTPUT);

  ticker.attach(1, tick);
    
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT);
  wifiManager.setConnectTimeout(CONNECT_TIMEOUT);
  wifiManager.setBreakAfterConfig(true);
  wifiManager.setWiFiAutoReconnect(true);
  
  if (drd.detectDoubleReset()) {
    drd.stop();
    DEBUG_PRINTLN("Double reset detected, starting config portal...");
    if (!wifiManager.startConfigPortal(HOSTNAMEOTA)) {
      DEBUG_PRINTLN("Failed to connect. Use ESP without WiFi.");
    }
  }
  
  rst_info *_reset_info = ESP.getResetInfoPtr();
  uint8_t _reset_reason = _reset_info->reason;
  DEBUG_PRINT("Boot-Mode: ");
  DEBUG_PRINTLN(_reset_reason);
  heartBeat = _reset_reason;

 /*
 REASON_DEFAULT_RST             = 0      normal startup by power on 
 REASON_WDT_RST                 = 1      hardware watch dog reset 
 REASON_EXCEPTION_RST           = 2      exception reset, GPIO status won't change 
 REASON_SOFT_WDT_RST            = 3      software watch dog reset, GPIO status won't change 
 REASON_SOFT_RESTART            = 4      software restart ,system_restart , GPIO status won't change 
 REASON_DEEP_SLEEP_AWAKE        = 5      wake up from deep-sleep 
 REASON_EXT_SYS_RST             = 6      external system reset 
  */
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  if (!wifiManager.autoConnect(AUTOCONNECTNAME, AUTOCONNECTPWD)) { 
    DEBUG_PRINTLN("Autoconnect failed connect to WiFi. Use ESP without WiFi.");
  }   

  WiFi.printDiag(Serial);
  
  sendNetInfoMQTT();
  
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
  ArduinoOTA.setHostname(HOSTNAMEOTA);

  ArduinoOTA.onStart([]() {
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
  sendStatisticMQTT(a);
  reconnect(a);
  
  DEBUG_PRINTLN(" Ready");
 
  ticker.detach();
  //keep LED on
  digitalWrite(STATUS_LED, HIGH);
  
  drd.stop();

  DEBUG_PRINTLN(F("Setup end."));
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


void startConfigPortal(void) {
  DEBUG_PRINTLN("START config portal");
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.startConfigPortal(HOSTNAMEOTA);
}

void stopConfigPortal(void) {
  DEBUG_PRINTLN("STOP config portal");
  wifiManager.stopConfigPortal();
}

bool sendStatisticMQTT(void *) {
  digitalWrite(STATUS_LED, LOW);
  DEBUG_PRINTLN(F("Statistic"));

  SenderClass sender;
  sender.add("VersionSW", VERSION);
  sender.add("Napeti",  ESP.getVcc());
  sender.add("HeartBeat", heartBeat++);
  sender.add("RSSI", WiFi.RSSI());
  
  DEBUG_PRINTLN(F("Calling MQTT"));
  
  sender.sendMQTT(mqtt_server, mqtt_port, mqtt_username, mqtt_key, mqtt_base);
  digitalWrite(STATUS_LED, HIGH);
  return true;
}

void sendDataMQTT1(void) {
  digitalWrite(STATUS_LED, LOW);
  DEBUG_PRINTLN(F("Data1"));

  SenderClass sender;
  sender.add("pulseLength1", pulseLengthMs1);
  
  DEBUG_PRINTLN(F("Calling MQTT"));
  
  sender.sendMQTT(mqtt_server, mqtt_port, mqtt_username, mqtt_key, mqtt_base);
  digitalWrite(STATUS_LED, HIGH);
  return;
}

void sendDataMQTT2(void) {
  digitalWrite(STATUS_LED, LOW);
  DEBUG_PRINTLN(F("Data2"));

  SenderClass sender;
  sender.add("pulseLength2", pulseLengthMs2);
  
  DEBUG_PRINTLN(F("Calling MQTT"));
  
  sender.sendMQTT(mqtt_server, mqtt_port, mqtt_username, mqtt_key, mqtt_base);
  digitalWrite(STATUS_LED, HIGH);
  return;
}

void sendNetInfoMQTT() {
  digitalWrite(BUILTIN_LED, LOW);
  DEBUG_PRINTLN(F("Net info"));

  SenderClass sender;
  sender.add("IP",              WiFi.localIP().toString().c_str());
  sender.add("MAC",             WiFi.macAddress());
  
  DEBUG_PRINTLN(F("Calling MQTT"));
  
  sender.sendMQTT(mqtt_server, mqtt_port, mqtt_username, mqtt_key, mqtt_base);
  digitalWrite(BUILTIN_LED, HIGH);
  return;
}

bool reconnect(void *) {
  if (!client.connected()) {
    DEBUG_PRINT("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_base, mqtt_username, mqtt_key)) {
      client.subscribe((String(mqtt_base) + "/#").c_str());
      DEBUG_PRINTLN("connected");
    } else {
      DEBUG_PRINT("failed, rc=");
      DEBUG_PRINTLN(client.state());
    }
  }
  return true;
}