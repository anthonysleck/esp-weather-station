/*
   ESP Weather Station
   Description:
   -Coming Soon!
   Notes:
   -Coming Soon!
   Components:
   -Coming Soon!
   Connections:
   -Coming Soon!
   Contact Info:
   email - anthony.sleck@gmail.com
   web - sleckconsulting.com
   github - https://github.com/anthonysleck
   Changelog:
   0.01 - new code
   0.02 - new code using async webserver; add'd firmware update page
*/

//inlcudes
#ifdef ESP32
  #include <ESPmDNS.h>
  #include <Update.h>
  #include <WiFi.h>
#else
  #include <Arduino.h>
  #include <ESPAsyncTCP.h>
  #include <ESP8266mDNS.h>
  #include <ESP8266WiFi.h>
  #include <Updater.h>
#endif
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESPAsyncWebServer.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include "credentials.h"
#include "webpages.h"

//constants-variables
const String ver = "ESP Weather Station - v0.02";
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme;
//float temperature, humidity, pressure, altitude;
const char *host = "ESP-WEATHER"; //change to your desired hostname
AsyncWebServer server(80);
size_t content_len;
int dBm;
float tempF;
float presEng;
float altF;
float relHum;


float getBME(){
  float temp = bme.readTemperature();
  float hum = bme.readHumidity();
  float pres = bme.readPressure() / 100.0F;
  float alt = bme.readAltitude(SEALEVELPRESSURE_HPA);

  //do english conversions
  tempF = temp *9/5 +32;
  presEng = pres *0.0002953;
  altF = alt *3.28084;
  relHum = hum;
}

void WiFiConnect(){
  #ifdef ESP32
  //start WiFi
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#else
  //start WiFi
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.hostname(host);
  Serial.println("");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Hostname: ");
  Serial.println(WiFi.hostname());
#endif
}

void RSSIdBm() {
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(WiFi.RSSI());
    }
    dBm = WiFi.RSSI();

  Serial.print("RSSI  = ");
  Serial.print(dBm);
  Serial.println("dBm ");

  delay(10000);
}

String readBMETemperatureF() {
  getBME();
  if(tempF == -196.00) {
    Serial.println("Failed to read from BME sensor");
    return "--";
  } else {
    Serial.print("Temperature Fahrenheit: ");
    Serial.println(tempF); 
  }
  return String(tempF);
}

String readBMEHum() {
  getBME();
  if(relHum == -196.00) {
    Serial.println("Failed to read from BME sensor");
    return "--";
  } else {
    Serial.print("Relative Humidity %: ");
    Serial.println(relHum); 
  }
  return String(relHum);
}

String processorData(const String& var){
  if(var == "TEMPERATUREF"){
    return readBMETemperatureF();
  }else if(var == "RELHUM"){
    return readBMEHum();
  }
  return String();
}

void rootServer(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", data_html, processorData);
  });

  server.on("/temperaturef", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", readBMETemperatureF().c_str());
  });

  server.on("/relhum", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", readBMEHum().c_str());
  });

  //start server
  server.begin();

  MDNS.begin(host);
  if(webInit()) MDNS.addService("http", "tcp", 80);
  Serial.println("HTTP server started");
  Serial.printf("Ready! To update firmware, open http://%s.local/firmware in your browser!\n", host);
  Serial.printf("Otherwise to open the root page, open http://%s.local in your browser!\n", host);
}

void handleUpdate(AsyncWebServerRequest *request){
  char* html = "<form method='POST' action='/doUpdate' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  request->send(200, "text/html", html);
}

void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
  if (!index){
    Serial.println("Update");
    content_len = request->contentLength();
    #ifdef ESP8266
    int cmd = (filename.indexOf("spiffs") > -1) ? U_FS : U_FLASH;
    #else
    int cmd = (filename.indexOf("spiffs") > -1) ? U_SPIFFS : U_FLASH;
    #endif
#ifdef ESP8266
    Update.runAsync(true);
    if (!Update.begin(content_len, cmd)) {
#else
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
#endif
      Update.printError(Serial);
    }
  }

  if (Update.write(data, len) != len) {
    Update.printError(Serial);
#ifdef ESP8266
  } else {
    Serial.printf("Progress: %d%%\n", (Update.progress()*100)/Update.size());
#endif
  }

  if (final) {
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
    response->addHeader("Refresh", "20");  
    response->addHeader("Location", "/");
    request->send(response);
    if (!Update.end(true)){
      Update.printError(Serial);
    } else {
      Serial.println("Update complete");
      Serial.flush();
      ESP.restart();
      }
    }
}

void printProgress(size_t prg, size_t sz) {
  Serial.printf("Progress: %d%%\n", (prg*100)/content_len);
}

boolean webInit(){
  server.on("/firmware", HTTP_GET, [](AsyncWebServerRequest *request){handleUpdate(request);});
  server.on("/doUpdate", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                  size_t len, bool final) {handleDoUpdate(request, filename, index, data, len, final);}
  );
  server.onNotFound([](AsyncWebServerRequest *request){request->send(404);});
  server.begin();
#ifdef ESP32
  Update.onProgress(printProgress);
#endif
}

void setup(){
  //start serial
  Serial.begin(115200);

  bool status;

  //print sketch information
  Serial.println("Created by Anthony Sleck");
  Serial.println("Email at anthony.sleck@gmail.com");
  Serial.println(ver);
  Serial.println("github - https://github.com/anthonysleck");

  //connect to sensor
  bme.begin(0x76);
  //getBME();

  //connect to WiFi
  WiFiConnect();

  //start server
  rootServer();
}

void loop(){
  //this is a sample code to output free heap every 5 seconds; this is a cheap way to detect memory leaks
  static unsigned long last = millis();
  if (millis() - last > 5000) {
    last = millis();
    Serial.printf("[MAIN] Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println(" ");
  }

  RSSIdBm();
}