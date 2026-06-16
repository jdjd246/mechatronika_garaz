#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <FS.h>
#include <LittleFS.h>

const char *ssid = "ssid";
const char *password = "pwd";

// Set Static IP address
IPAddress local_IP(192, 168, 1, 184);
// Set Gateway IP address
IPAddress gateway(192, 168, 1, 1);

IPAddress subnet(255, 255, 0, 0);

// TO DO: dopasować numery pinów do czujników
const int ALARM_PINS[] = { 12,13,14,27 };
const int NUM_ALARM_PINS = sizeof(ALARM_PINS) / sizeof(ALARM_PINS[0]);

const int ALARM_OUT_PIN = 2;

bool alarmArmed = false;
bool alarmTripped = false;
bool alarmsState[NUM_ALARM_PINS];

#define sensorDrzwi alarmsState[0]
#define sensorBrama alarmsState[1]
#define sensorOkno alarmsState[2]
#define sensorRuch alarmsState[3]

AsyncWebServer server(80);

void handleSensors() {
  for (int i = 0;i < NUM_ALARM_PINS;i++) {
    alarmsState[i] = (digitalRead(ALARM_PINS[i]) == HIGH);
    if (alarmArmed  && alarmsState[i]) {
      alarmTripped = true;
    }
  }
}

String processor(const String& var){
  Serial.println(var);
  if(var == "ALARM"){
    return getAlarm();
  }
  else if (var == "DRZWI"){
    return getOtwarteZ(sensorDrzwi);
  }
  else if (var == "BRAMA"){
    return getOtwarteN(sensorBrama);
  }
  else if (var == "OKNO"){
    return getOtwarteN(sensorOkno);
  }
  else if (var == "RUCH"){
    return getRuch(sensorRuch);
  }
  return String();
}

String getAlarm() {
  if (alarmTripped) {
    return String("AKTYWOWANY");
  }
  else if (alarmArmed) {
    return String("Uzbrojony");
  }
  else {
    return String("Nieaktywny");
  }
}

String getOtwarteN(bool state) {
  if (state) {
    return String("Otwarte");
  }
  else {
    return String("Zamknięte");
  }
}

String getOtwarteZ(bool state) {
  if (state) {
    return String("Otwarta");
  }
  else {
    return String("Zamknięta");
  }
}

String getRuch(bool state) {
  if (state) {
    return String("Wykryto ruch");
  }
  else {
    return String("Brak ruchu");
  }
}

void setup() {
  Serial.begin(115200);

  for(int i = 0; i < NUM_ALARM_PINS; i++) pinMode(ALARM_PINS[i],INPUT);

  // Initialize LittleFS
  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }

  // Configures static IP address
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }

  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", String(), false, processor);
  });
  
  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/style.css", "text/css");
  });

  server.on("/garaz.png", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(LittleFS, "/garaz.png", "image/png");
  });

  // Route to alarm reset
  server.on("/alarmreset", HTTP_GET, [](AsyncWebServerRequest *request){
    alarmTripped = false;
    request->send(LittleFS, "/index.html", String(), false, processor);
  });

  // Route to alarm arm
  server.on("/alarmarm", HTTP_GET, [](AsyncWebServerRequest *request){
    alarmArmed = true;
    request->send(LittleFS, "/index.html", String(), false, processor);
  });

  server.on("/alarm", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", getAlarm().c_str());
  });

  server.on("/drzwi", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", getOtwarteN(sensorDrzwi).c_str());
    });

  server.on("/brama", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", getOtwarteZ(sensorBrama).c_str());
    });

  server.on("/okno", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", getOtwarteN(sensorOkno).c_str());
    });

  server.on("/ruch", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", getOtwarteN(sensorRuch).c_str());
  });

  server.begin();
}

void loop() {
  handleSensors();

  digitalWrite(ALARM_OUT_PIN, alarmTripped ? HIGH : LOW);
}
