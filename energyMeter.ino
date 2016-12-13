#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "FS.h"

const char *ssid = "Datlovo";
const char *password = "Nu6kMABmseYwbCoJ7LyG";

ESP8266WebServer server(80);

#define AIO_SERVER      "178.77.238.20"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "datel"
#define AIO_KEY         "hanka12"

WiFiClient client;

Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

unsigned int const sendTimeDelay=1000;
signed long lastSendTime = sendTimeDelay * -1;

uint32_t pulseCount = 0;
uint32_t pulseMillisOld = 0;

/****************************** Feeds ***************************************/
Adafruit_MQTT_Publish verSW = Adafruit_MQTT_Publish(&mqtt,  "/flat/EnergyMeter/esp09T/VersionSW");
Adafruit_MQTT_Publish hb = Adafruit_MQTT_Publish(&mqtt,  "/flat/EnergyMeter/esp09T/HeartBeat");
Adafruit_MQTT_Publish pulse = Adafruit_MQTT_Publish(&mqtt,  "/flat/EnergyMeter/esp09T/Pulse");
Adafruit_MQTT_Publish pulseLength = Adafruit_MQTT_Publish(&mqtt,  "/flat/EnergyMeter/esp09T/pulseLength");

// Setup a feed called 'onoff' for subscribing to changes.
//Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/onoff");

#define SERIALSPEED 115200

void MQTT_connect(void);

const byte ledPin = 13;
const byte interruptPin = 2;

void handleRoot()
{
  
	char temp[450];
	int sec = millis() / 1000;
	int min = sec / 60;
	int hr = min / 60;

	snprintf(temp, 450,
		"<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP8266 Temperatures</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Teploty chata</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
	<p>Teplota venku: %02d.%01d&deg;C</p>\
	<p>Teplota doma: %02d.%01d&deg;C</p>\
  </body>\
</html>",

hr, min % 60, sec % 60, 0, 0
);
	server.send(200, "text/html", temp);
}

void handleNotFound() {
	String message = "File Not Found\n\n";
	message += "URI: ";
	message += server.uri();
	message += "\nMethod: ";
	message += (server.method() == HTTP_GET) ? "GET" : "POST";
	message += "\nArguments: ";
	message += server.args();
	message += "\n";

	for (uint8_t i = 0; i < server.args(); i++) {
		message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
	}

	server.send(404, "text/plain", message);
}

float versionSW          = 0.71;
char versionSWString[]   = "EnergyMeter v";
byte heartBeat = 10;



void setup() {
  Serial.begin(SERIALSPEED);
  Serial.print(versionSWString);
  Serial.println(versionSW);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);
  
  WiFi.begin(ssid, password);

	// Wait for connection
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	Serial.println("");
	Serial.print("Connected to ");
	Serial.println(ssid);
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());
  
  SPIFFS.begin();
  
  server.on("/", handleRoot);
	server.on("/inline", []() {
		server.send(200, "text/plain", "this works as well");
	});
	server.onNotFound(handleNotFound);
	server.begin();
	Serial.println("HTTP server started");

  //v klidu 0, kladny pulz po dobu xx ms
  pinMode(interruptPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(interruptPin), pulseCountEvent, RISING);
  digitalWrite(ledPin, LOW);
  
  // open config file for reading
  File f;
  if (SPIFFS.exists("/config.ini")) {
    f = SPIFFS.open("/config.ini", "r");
    if (!f) {
      Serial.println("file open failed");
    } else {
      Serial.println("====== Reading config from SPIFFS file =======");
      String s = f.readStringUntil('\n');
      Serial.print("Pocet pulzu z config.ini:");
      Serial.println(s);
      pulseCount = s.toInt();
    }
  } else {
    f = SPIFFS.open("/config.ini", "w");
    if (!f) {
      Serial.println("file open failed");
    } else {
      Serial.println("====== Create new config.ini SPIFFS file =======");
      f.println(0);
    }
  }
}

uint32_t x=0;

void loop() {
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  // Adafruit_MQTT_Subscribe *subscription;
  // while ((subscription = mqtt.readSubscription(5000))) {
    // if (subscription == &onoffbutton) {
      // Serial.print(F("Got: "));
      // Serial.println((char *)onoffbutton.lastread);
    // }
  // }

 if (millis() - lastSendTime >= sendTimeDelay) {
    lastSendTime = millis();
    if (! verSW.publish(versionSW)) {
      Serial.println("failed");
    } else {
      Serial.println("OK!");
    }
    if (! hb.publish(heartBeat++)) {
      Serial.println("failed");
    } else {
      Serial.println("OK!");
      if (heartBeat>1) {
        heartBeat = 0;
      }
    }
    pulseCount++;
    Serial.println(pulseCount);
    if (! pulse.publish(pulseCount)) {
      Serial.println("failed");
    } else {
      Serial.println("OK!");
    }
  }
  // ping the server to keep the mqtt connection alive
  // NOT required if you are publishing once every KEEPALIVE seconds
  /*
  if(! mqtt.ping()) {
    mqtt.disconnect();
  }
  */

  server.handleClient();
}


// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}

void pulseCountEvent() {
  if (millis() - pulseMillisOld>100) {
    pulseCount++;
    Serial.println(pulseCount);
    if (! pulse.publish(pulseCount)) {
      Serial.println("failed");
    } else {
      Serial.println("OK!");
    }
    if (pulseMillisOld>0) {
      if (! pulseLength.publish((uint32_t)millis() - pulseMillisOld)) {
        Serial.println("failed");
      } else {
        Serial.println("OK!");
      }
    }
  }
  pulseMillisOld = millis();
}