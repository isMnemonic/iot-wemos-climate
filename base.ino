#include <Timer.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

/********************** SENSORS ***********************************/
#define READ A0

#define DHT_PIN D1
#define DHT_TYPE DHT22

#define LED_ERR D7
#define LED_OK D8

/********************** WIFI ACCESS POINT *************************/
#define WIFI_SSID "..."
#define WIFI_PWD "..."

/********************** MQTT BROKER *******************************/
#define MQTT_ADDRESS "..."
#define MQTT_PORT 8883
#define MQTT_USER "..."
#define MQTT_PWD "..."

/********************** GLOBAL STATS ******************************/
int lux[60];
int luxTotal = 60;
int luxCurrentIndex = 0;

int dhtId;
int luxId;

Timer tDht;
Timer tLux;

DHT dht(DHT_PIN, DHT_TYPE);
WiFiClientSecure client;

Adafruit_MQTT_Client mqtt(&client, MQTT_ADDRESS, MQTT_PORT, MQTT_USER, MQTT_PWD);
Adafruit_MQTT_Publish mqttTemperature = Adafruit_MQTT_Publish(&mqtt, "topic/dht/temperature/set");
Adafruit_MQTT_Publish mqttHumidity = Adafruit_MQTT_Publish(&mqtt, "topic/dht/humidity/set");
Adafruit_MQTT_Publish mqttHeatIndex = Adafruit_MQTT_Publish(&mqtt, "topic/dht/heat/set");
Adafruit_MQTT_Publish mqttLux = Adafruit_MQTT_Publish(&mqtt, "topic/lux/set");

static const char *fingerprint PROGMEM="E6 88 A8 B3 20 F0 AD A7 6F 8C 2A 01 5F 03 8E 58 F1 DB A7 95";

/*************************** Sketch Code ***************************/
// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
void MQTT_connect();

void setup() {
  WIFI_Connect();
  dht.begin();
  dhtId = tDht.every(120000, getDhtData, (void*)0);
  luxId = tLux.every(1000, getLuxData, (void*)0);
}

void loop() {
  ArduinoOTA.handle();
  MQTTConnect();
  tDht.update();
  tLux.update();
}

void WIFI_Connect() {
  delay(10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PWD);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.setHostname("iot-kitchen");
  ArduinoOTA.setPassword((const char *)"8z9Fyp5UwMSP6h2");
  ArduinoOTA.begin();
  client.setFingerprint(fingerprint);
}

void MQTTConnect() {
  int8_t checkMqttConnection;
  uint8_t retries = 5;

  /* Check if we are connected and continue if so.*/
  if (mqtt.connected()) {
    return;
  }  

  while ((checkMqttConnection = mqtt.connect()) != 0) {
      mqtt.disconnect();
      delay(1000);
      retries--;

      /* Not yet connected to MQTT so light up warning led */
      digitalWrite(LED_OK, LOW);
      digitalWrite(LED_ERR, HIGH);
      
      /* Unable to connect to MQTT so we need to reset */
      if (retries == 0) {
        ESP.restart();
      }
  }

  /* MQTT is now connected */
  digitalWrite(LED_OK, HIGH);
  digitalWrite(LED_ERR, LOW);

}

void getDhtData(void *context) {
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    if (isnan(humidity) || isnan(temperature)) {
      digitalWrite(LED_OK, LOW);
      digitalWrite(LED_ERR, HIGH);
    } else {
      float heatindex = dht.computeHeatIndex(temperature, humidity, false);
      mqttTemperature.publish(temperature);
      mqttHumidity.publish(humidity);
      mqttHeatIndex.publish(heatindex);
    }
}

void getLuxData(void *context) {
  if(luxCurrentIndex == 60) {
    float _lux;
    for(int i = 0; i<=luxTotal; i++) {
      _lux += lux[i];
    }
    mqttLux.publish(_lux /= luxTotal);
    luxCurrentIndex = 0;
  } else {
    lux[luxCurrentIndex] = analogRead(READ);
    luxCurrentIndex++;
  }
}
