#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "config.h"
#include "sensor.h"

WebServer server(HTTP_PORT);

static BmpReading lastBmp = {0.0f, 0.0f, 0.0f,false}; 

void handleRoot() {
  char msg[2000];

  snprintf(msg, 2000,
           "<html>\
  <head>\
    <meta http-equiv='refresh' content='4'/>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
    <link rel='stylesheet' href='https://use.fontawesome.com/releases/v5.7.2/css/all.css' integrity='sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr' crossorigin='anonymous'>\
    <title>Ground Station</title>\
    <style>\
    html { font-family: Arial; display: inline-block; margin: 0px auto; text-align: center;}\
    h2 { font-size: 3.0rem; }\
    p { font-size: 3.0rem; }\
    .units { font-size: 1.2rem; }\
    .labels{ font-size: 1.5rem; vertical-align:middle; padding-bottom: 15px;}\
    </style>\
  </head>\
  <body>\
      <h2>Ground Station</h2>\
      <p>\
        <i class='fas fa-tachometer-alt' style='color:#555;'></i>\
        <span class='labels'>Pressure</span>\
        <span>%.2f</span>\
        <sub class='units'>hPa</sub>\
      </p>\
      <p>\
        <i class='fas fa-thermometer-half' style='color:#e74c3c;'></i>\
        <span class='labels'>Temperature</span>\
        <span>%.2f</span>\
        <sub class='units'>&deg;C</sub>\
      </p>\
      <p>\
        <i class='fas fa-mountain' style='color:#4a90d9;'></i>\
        <span class='labels'>Altitude</span>\
        <span>%.2f</span>\
        <sub class='units'>m</sub>\
      </p>\
  </body>\
</html>",
          lastBmp.pressure,
          lastBmp.temperatureC,
          lastBmp.altitudeM
          );
  server.send(200, "text/html", msg);
}

bool initServer() {
  // Makes ESP work as both an access point and a station point.
  WiFi.mode(WIFI_AP_STA);
  // Rocket -> phone
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("[server] AP IP: ");
  Serial.print(WiFi.softAPIP());
  // Phone's hotspot -> rocket
  WiFi.begin(NETWORK_SSID, NETWORK_PASS);
  Serial.println("");
  server.on("/", handleRoot);
  server.begin();
  
  unsigned long wifiStart = millis();
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifiStart >= WIFI_TIMEOUT_MS) {
      Serial.println("[server] WiFi timed out.");
      return false;
    }
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.print("[server] Connected to ");
  Serial.println(NETWORK_SSID);
  Serial.print("[server] IP address: ");
  Serial.println(WiFi.localIP());
  
  if (MDNS.begin("esp32")) {
    Serial.println("[server] MDNS responder started");
  }
  
  
  Serial.println("[server] HTTP server started");
  return true;
}

void updateBMPReading(const BmpReading& reading) {
  lastBmp = reading;
}

void handleClients() {
  server.handleClient();
}