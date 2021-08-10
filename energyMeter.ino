/*ENERGY Meter
--------------------------------------------------------------------------------------------------------------------------
Petr Fory pfory@seznam.cz
GIT - https://github.com/pfory/energyMeter
//ESP8266-01
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

//uint32_t      pulseCount                          = 0;
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

void ICACHE_RAM_ATTR pulseCountEvent() {
  if (millis() - pulseMillisOld>50) {
    pulseLengthMs=millis() - pulseMillisOld;
    pulseMillisOld = millis();
    DEBUG_PRINTLN(pulseLengthMs);
    pulseNow=true;
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
  }
  
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  DEBUG_PRINTLN("Entered config mode");
  DEBUG_PRINTLN(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  DEBUG_PRINTLN(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
}

ADC_MODE(ADC_VCC); //vcc read

void tick() {
  //toggle state
  int state = digitalRead(STATUS_LED);  // get the current state of GPIO1 pin
  digitalWrite(STATUS_LED, !state);          // set pin to the opposite state
}

WiFiClient espClient;
PubSubClient client(espClient);

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
  
    if (drd.detectDoubleReset()) {
    DEBUG_PRINTLN("Double reset detected, starting config portal...");
    ticker.attach(0.2, tick);
    if (!wifiManager.startConfigPortal(HOSTNAMEOTA)) {
      DEBUG_PRINTLN("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
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

  WiFi.printDiag(Serial);
    
  if (!wifiManager.autoConnect(AUTOCONNECTNAME, AUTOCONNECTPWD)) { 
    DEBUG_PRINTLN("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  } 

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
  pinMode(INTERRUPTPIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(INTERRUPTPIN), pulseCountEvent, RISING);
  
#ifdef timers
  //setup timers
  timer.every(SENDSTAT_DELAY, sendStatisticMQTT);
#endif

  void * a;
  sendStatisticMQTT(a);
  
  DEBUG_PRINTLN(" Ready");
 
  ticker.detach();
  //keep LED on
  digitalWrite(STATUS_LED, HIGH);
  
  drd.stop();

  DEBUG_PRINTLN(F("Setup end."));
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

  reconnect();
  client.loop();
} //loop

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

bool sendDataMQTT(void) {
  digitalWrite(STATUS_LED, LOW);
  DEBUG_PRINTLN(F("Data"));

  SenderClass sender;
  sender.add("pulseLength", pulseLengthMs);
  //sender.add("Pulse", pulseCount);
  
  DEBUG_PRINTLN(F("Calling MQTT"));
  
  sender.sendMQTT(mqtt_server, mqtt_port, mqtt_username, mqtt_key, mqtt_base);
  sendNetInfoMQTT();
  digitalWrite(STATUS_LED, HIGH);
  return true;
}

void sendNetInfoMQTT() {
  digitalWrite(BUILTIN_LED, LOW);
  //printSystemTime();
  DEBUG_PRINTLN(F("Net info"));

  SenderClass sender;
  sender.add("IP",              WiFi.localIP().toString().c_str());
  sender.add("MAC",             WiFi.macAddress());
  
  DEBUG_PRINTLN(F("Calling MQTT"));
  
  sender.sendMQTT(mqtt_server, mqtt_port, mqtt_username, mqtt_key, mqtt_base);
  digitalWrite(BUILTIN_LED, HIGH);
  return;
}

// bool generatePulse(void *) {
  // pulseNow=true;
  // return true;
// }

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
     if (lastConnectAttempt == 0 || lastConnectAttempt + connectDelay < millis()) {
    DEBUG_PRINT("Attempting MQTT connection...");
      // Attempt to connect
      if (client.connect(mqtt_base, mqtt_username, mqtt_key)) {
        DEBUG_PRINTLN("connected");
        client.subscribe((String(mqtt_base) + "/#").c_str());
      } else {
        lastConnectAttempt = millis();
        DEBUG_PRINT("failed, rc=");
        DEBUG_PRINTLN(client.state());
      }
    }
  }
}