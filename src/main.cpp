#include <Arduino.h>
#include "DHTesp.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#ifdef ESP32
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";
const String apiKey = "YOUR_API_KEY";
const String host = "https://api.info-dash.lat/v1";
const int INTERVAL = 60000;
const int DHTPin = 4;

float humidity = 0;
float temperature = 0;
String sendJson = "";

DHTesp dht;

String getSerialID()
{
  uint64_t chipid = ESP.getEfuseMac();
  uint32_t chip = (uint32_t)(chipid >> 32);
  return (String)chip;
}

bool connectToWifi()
{
  static bool tick = false;
  byte attempt = 0;

  Serial.println("Info: Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    attempt++;
    tick = !tick;
    if (attempt > 20)
      return false;
    digitalWrite(LED_BUILTIN, tick);
    delay(500);
  }
  digitalWrite(LED_BUILTIN, WiFi.status() == WL_CONNECTED ? HIGH : LOW);
  return true;
}

String buildSendJson(String response)
{
  DynamicJsonDocument jsonResponse(1024);
  DynamicJsonDocument jsonPayload(1024);
  deserializeJson(jsonResponse, response);

  jsonPayload["serial"] = getSerialID();
  jsonPayload["apiKey"] = apiKey;

  for (size_t i = 0; i < jsonResponse["stations"][0]["sensors"].size(); i++)
    jsonPayload["payloads"][i]["sensorId"] = jsonResponse["stations"][0]["sensors"][i]["sensorId"];

  String buildPayload;
  serializeJson(jsonPayload, buildPayload);
  jsonPayload.clear();
  jsonResponse.clear();
  return buildPayload;
}

bool connectToServer()
{
  Serial.println("Info: Connecting to server...");
  HTTPClient http;
  String url = host + "/network/access/" + apiKey;

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", apiKey);

  int httpCode = http.GET();

  if (httpCode <= 0)
    Serial.printf("Info: HTTP GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  else
  {
    Serial.printf("Info: HTTP GET... code = %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK)
    {
      sendJson = buildSendJson(http.getString());
      return true;
    }
  }
  http.end();
  return false;
}

void readDHT()
{
  temperature = dht.getTemperature();
  humidity = dht.getHumidity();
  Serial.print("Info: Temperature: ");
  Serial.print(temperature);
  Serial.print("°C");
  Serial.print(" Humidity: ");
  Serial.print(humidity);
  Serial.println("%");
}

String getPayload()
{
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, sendJson);

  doc["payloads"][0]["value"] = (int)temperature;
  doc["payloads"][1]["value"] = (int)humidity;

  String payload;
  serializeJson(doc, payload);
  doc.clear();
  return payload;
}

void sendDataByHttp()
{
  HTTPClient http;
  http.begin(host);
  http.addHeader("Content-Type", "application/json");
  String payload = getPayload();
  int httpCode = http.POST(payload);
  Serial.println("Info: HTTP POST..code = " + (String)httpCode);
  http.end();
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("Info: Starting...");
  Serial.println("Info: Serial ID: " + getSerialID());
  dht.setup(DHTPin, DHTesp::DHT22);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  connectToWifi();
}

void loop()
{
  static bool isConnectedToServer = false;
  static unsigned long currentMillis = millis();

  if (millis() - currentMillis > INTERVAL)
  {
    bool isWiFiConnected = WiFi.status() == WL_CONNECTED;
    currentMillis = millis();
    readDHT();
    digitalWrite(LED_BUILTIN, isWiFiConnected ? HIGH : LOW);

    if (isWiFiConnected)
    {
      if (isConnectedToServer)
        sendDataByHttp();
      else
        isConnectedToServer = connectToServer();
    }
  }
}