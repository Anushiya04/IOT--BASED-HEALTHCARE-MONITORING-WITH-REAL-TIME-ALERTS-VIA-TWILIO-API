#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "DHT.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiClientSecure.h>
#include <Base64.h>
#include <time.h>

#define DHTTYPE DHT22 
#define DHTPIN 18
#define DS18B20 5
#define REPORTING_PERIOD_MS 1000

float temperature, humidity, BPM, SpO2, bodytemperature;

const char* ssid = "Redmi 10A";  
const char* password = "palanikumar";

DHT dht(DHTPIN, DHTTYPE); 
uint32_t tsLastReport = 0;
OneWire oneWire(DS18B20);
DallasTemperature sensors(&oneWire);

WebServer server(80);
WiFiClientSecure client;

const char* TWILIO_ACCOUNT_SID = "ACb23a2be5baf1bcfc1a61b7b1c054f538";
const char* TWILIO_AUTH_TOKEN = "e74391428927030f56b5f119f1b113d1";
const char* TWILIO_FROM_NUMBER = "+17316137919";
const char* TWILIO_TO_NUMBER = "+919677413643";

#define BODY_TEMP_THRESHOLD 37.0
#define ROOM_TEMP_THRESHOLD 35.0

const char* twilio_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n" \
"MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n" \
"2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n" \
"1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n" \
"q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n" \
"tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n" \
"vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n" \
"BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n" \
"5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n" \
"1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n" \
"NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n" \
"Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n" \
"8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n" \
"pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n" \
"MrY=\n" \
"-----END CERTIFICATE-----\n";

const long gmtOffset_sec = 5 * 3600 + 1800; 
const int daylightOffset_sec = 0;  

void syncTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    now = time(nullptr);
  }
}

void sendSMS(String message) {
  client.setCACert(twilio_cert);

  if (!client.connect("api.twilio.com", 443)) {
    Serial.println("Connection to Twilio failed");
    return;
  }

  String authHeader = "Basic " + base64::encode(String(TWILIO_ACCOUNT_SID) + ":" + TWILIO_AUTH_TOKEN);
  String postData = "To=" + String(TWILIO_TO_NUMBER) + "&From=" + String(TWILIO_FROM_NUMBER) + "&Body=" + message;

  client.println("POST /2010-04-01/Accounts/" + String(TWILIO_ACCOUNT_SID) + "/Messages.json HTTP/1.1");
  client.println("Host: api.twilio.com");
  client.println("Authorization: " + authHeader);
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.println("Content-Length: " + String(postData.length()));
  client.println();
  client.println(postData);

  while (client.connected()) {
    String response = client.readString();
    Serial.println(response);
    break;
  }
}

void checkTemperatureAndSendSMS(float bodyTemp, float roomTemp) {
  String message = "";

  if (bodyTemp > BODY_TEMP_THRESHOLD) {
    message += "Warning: Body temperature is high (" + String(bodyTemp) + "°C). ";
  }
  
  if (roomTemp > ROOM_TEMP_THRESHOLD) {
    message += "Room temperature is high (" + String(roomTemp) + "°C). ";
  }
  
  if (message != "") {
    message += "Please take necessary action.";
    sendSMS(message);
  }
}

void setup() {
  Serial.begin(57600);
  delay(100);   
  dht.begin();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  
  Serial.println(WiFi.localIP());

  syncTime();

  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("HTTP server started");

  sensors.begin();
}

void loop() {
  server.handleClient();
  delay(1000);

  // Read humidity and temperature from DHT22
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  // Read the BPM and SpO2 from the UART connection
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    // Parse the line to get BPM and SpO2
    int bpmIndex = line.indexOf(':') + 2; // Skip past "Heart Rate: "
    int spO2Index = line.indexOf('/', bpmIndex) + 10; // Skip past "bpm / SpO2: "
    
    if (bpmIndex > 1 && spO2Index > 1) { // Check if indices are valid
        BPM = line.substring(bpmIndex, line.indexOf(' ', bpmIndex)).toFloat();
        SpO2 = line.substring(spO2Index, line.indexOf('%', spO2Index)).toFloat();
    }
}


  // Read body temperature from DS18B20
  sensors.requestTemperatures(); 
  bodytemperature = sensors.getTempCByIndex(0);

  // Report data every REPORTING_PERIOD_MS
  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
    tsLastReport = millis();
    Serial.print("Heart rate: ");
    Serial.print(BPM);
    Serial.print(" bpm  ");
    //Serial.print(SpO2);
    Serial.print("% / Body temperature: ");
    Serial.print(bodytemperature);
    Serial.print("°C / Room temperature: ");
    Serial.print(temperature);
    Serial.print("°C / Humidity: ");
    Serial.print(humidity);
    Serial.println("%");

    checkTemperatureAndSendSMS(bodytemperature, temperature);
  }
}

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML(temperature, humidity, BPM, SpO2, bodytemperature)); 
}

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

String SendHTML(float temperature, float humidity, float BPM, float SpO2, float bodytemperature) {
  String html = "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  html += "<title>Patient Health Monitoring</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>body { background-color: #fff; font-family: sans-serif; color: #333333; font: 14px Helvetica, sans-serif; box-sizing: border-box;}</style>";
  html += "<script>";
  html += "setInterval(loadDoc,1000);";
  html += "function loadDoc() {";
  html += "var xhttp = new XMLHttpRequest();";
  html += "xhttp.onreadystatechange = function() {";
  html += "if (this.readyState == 4 && this.status == 200) {";
  html += "document.body.innerHTML = this.responseText}";
  html += "};";
  html += "xhttp.open('GET', '/', true);";
  html += "xhttp.send();";
  html += "}";
  html += "</script>";
  html += "</head>";
  html += "<body>";
  html += "<div id='page'>";
  html += "<h1>Health Monitoring System</h1>";
  html += "<div class='sensor'><span>Room Temperature: </span>" + String((int)temperature) + "°C</div>";
  html += "<div class='sensor'><span>Humidity: </span>" + String((int)humidity) + "%</div>";
  html += "<div class='sensor'><span>Heart Rate: </span>" + String((int)BPM) + " bpm</div>";
  //html += "<div class='sensor'><span>SpO2: </span>" + String((int)SpO2) + "%</div>";
  html += "<div class='sensor'><span>Body Temperature: </span>" + String((int)bodytemperature) + "°C</div>";
  html += "</div>";
  html += "</body>";
  html += "</html>";
  return html;
}
