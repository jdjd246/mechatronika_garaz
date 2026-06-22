#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <FS.h>
#include <LittleFS.h>
#include <time.h>
const char *ssid = "ssid";
const char *password = "pwd";

// Set Static IP address
IPAddress local_IP(192, 168, 1, 184);
// Set Gateway IP address
IPAddress gateway(192, 168, 1, 1);

IPAddress subnet(255, 255, 255, 0);

// NTP
const char* ntpServer = "pool.ntp.org";

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

int logCounter = 0;

AsyncWebServer server(80);

void truncateFile(fs::FS &fs, const char * path){
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print("")){
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
  file.close();
}

void loadLogCounter()
{
    File file = LittleFS.open("/counter.txt", FILE_READ);

    if(file)
    {
        logCounter = file.readString().toInt();
        file.close();
    }
}

void saveLogCounter()
{
    File file = LittleFS.open("/counter.txt", FILE_WRITE);

    if(file)
    {
        file.print(logCounter);
        file.close();
    }
}

void addLog(const String& type, const String& msg)
{
  File file = LittleFS.open("/logs.txt", FILE_APPEND);

  logCounter++;

  if (file)
    {
        file.printf("%d|%s|%s|%s\n",
                    logCounter,
                    getDateTime().c_str(),
                    type.c_str(),
                    msg.c_str());

        file.close();

        
        saveLogCounter();
  }
  else {
    logCounter--;
    }
}

String processor(const String& var) {
  Serial.println(var);
  if(var == "ALARM"){
    return getAlarm();
  }
  else if (var == "DRZWI"){
    return getOtwarteN(sensorDrzwi);
  }
  else if (var == "BRAMA"){
    return getOtwarteZ(sensorBrama);
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

String getArmed() {
  if (alarmArmed) {
    return String("1");
  }
  else {
    return String("0");
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

String getDateTime()
{
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo))
        return "";

    char buffer[20];

    strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%d %H:%M:%S",
        &timeinfo
    );

    return String(buffer);
}

void handleSensors() {
  for (int i = 0;i < NUM_ALARM_PINS;i++) {
    alarmsState[i] = (digitalRead(ALARM_PINS[i]) == HIGH);
    if (!alarmTripped && alarmArmed && alarmsState[i]) {
      alarmTripped = true;
      addLog("ALARM","Wykryto wtargnięcie!");
    }
  }
}

void setup() {
  Serial.begin(115200);

  for(int i = 0; i < NUM_ALARM_PINS; i++) pinMode(ALARM_PINS[i],INPUT);
  pinMode(ALARM_OUT_PIN, OUTPUT);

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
  // NTP
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();

  configTime(0, 0, ntpServer);

  struct tm timeinfo;

  int retries = 20;
  while (retries--)
  {
    if(getLocalTime(&timeinfo))
    {
      Serial.println("Time synchronised");
      break;
    }
    else
    {
      Serial.println("Time synchronisation failed");
    }
    delay(500);
  }

  loadLogCounter();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", String(), false, processor);
  });
  
  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/style.css", "text/css");
  });

  server.on("/garaz.jpg", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(LittleFS, "/garaz.jpg", "image/jpeg");
  });

  // Route to alarm reset
  server.on("/alarmreset", HTTP_GET, [](AsyncWebServerRequest *request){
    alarmTripped = false;
    addLog("RESET","Skasowano alarm");
    request->send(LittleFS, "/index.html", String(), false, processor);
  });

  // Route to alarm arm
  server.on("/alarmarmtoggle", HTTP_GET, [](AsyncWebServerRequest *request){
    if (alarmArmed) {
      alarmArmed = false;
      addLog("DISARM", "Wyłączono detekcję");
    }
    else {
      alarmArmed = true;
      addLog("ARM", "Włączono detekcję");
    }

    request->send(LittleFS, "/index.html", String(), false, processor);
    });

  server.on("/clearlogs", HTTP_GET, [](AsyncWebServerRequest *request){
    truncateFile(LittleFS, "/logs.txt");
    logCounter = 0;
    saveLogCounter();
    request->send(LittleFS, "/index.html", String(), false, processor);
  });

  server.on("/alarm", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", getAlarm().c_str());
    });

  server.on("/detection", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", getArmed().c_str());
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
    request->send(200, "text/plain", getRuch(sensorRuch).c_str());
    });

  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request){
    int lastId = 0;

    if(request->hasParam("lastId"))
        lastId = request->getParam("lastId")->value().toInt();

    AsyncResponseStream *response = request->beginResponseStream("text/plain");

    File file = LittleFS.open("/logs.txt", FILE_READ);

    while(file.available())
    {
        String line = file.readStringUntil('\n');

        int pos = line.indexOf('|');
        if(pos < 0)
            continue;

        int id = line.substring(0, pos).toInt();

        if (id > lastId) {
          response->println(line);
        }
    }
    file.close();

    request->send(response);
    });

  server.begin();
}

void loop() {
  handleSensors();

  digitalWrite(ALARM_OUT_PIN, alarmTripped ? HIGH : LOW);
}
