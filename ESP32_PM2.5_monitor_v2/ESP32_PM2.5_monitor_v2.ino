/*
- Binh Nguyen,September 30, 2020
- Using NTP time for clock
- PMS7003 (Plantower) for PM2.5 sensor
- HDC1080 (TI) for temperature and relative humidity
- public data to MQTT server
- log data using Python script
- include update over air (OTA)
*/

/*______________        _LIBRARIES FOR EACH SENSOR _        _______________*/
#include <Wire.h>    // I2C library
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "SSD1306.h"
#include <pms.h>
#include "ClosedCube_HDC1080.h"

Pmsx003 pms(13, 15);
SSD1306  oled(0x3c, 5, 4);
WiFiClient espClient;
PubSubClient client(espClient);
ClosedCube_HDC1080 hdc1080;

/*______________        _ WIFI and MQTT INFORMATION  _        _______________*/
#define wifi_ssid "your_wifi_name"
#define wifi_password "your_wifi_password"
#define DATE "v2.Sep30"
#define mqtt_server "MQTT_IP"
#define mqtt_user "user" 
#define mqtt_password "mqtt_password"
#define DATE "v2.Sep30"
/*______________   MQTT TOPICS      _______________*/
String HOSTNAME;
#define SENSORNAME "dust_work"
#define publish_topic "pms/dust_work"
#define OTApassword "ota_password"
int OTAport = 8266;

// for NTP time
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7*3600; // for GMT+7 
const int   daylightOffset_sec = 0; // apply to US
uint16_t INVL = 60;

/*______________        _ GLOBAL VARIABLES_        _______________*/
uint8_t pm1_0_ac, pm2_5_ac,pm10_ac;
int32_t lastSampling = -1000;
uint32_t um03;
uint16_t um05, um1, um2_5, um5, um10;
uint32_t uptime;
char hm[7];
float t_hdc, h_hdc;
String ip;

/*______________        _ START SETUP _        _______________*/
void setup() {

  Serial.begin(115200);
  Serial.println("\nStarting :::: " + String(SENSORNAME));
  Wire.begin(5,4); 
  oled.init();
  oled.flipScreenVertically();

  setup_wifi();
  setup_OTA();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  pms.begin();
  pms.waitForData(Pmsx003::wakeupTime);
  pms.write(Pmsx003::cmdModePassive);
  hdc1080.begin(0x40);
  display_data("Air Monitior");
  Serial.println("LEAVING SETUP LOOP");
  delay(2000);
}

/*______________        _ START MAIN LOOP _        _______________*/
void loop() {

  if (!client.connected()){
    reconnect();
  }
  client.loop();
  ArduinoOTA.handle();
  
  uptime = round(millis()/1000L);
  if ((uptime%86400L)==0){
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  }
  if ((uptime -lastSampling) > INVL){
    read_pms();
  }
  delay(1000);

}

/*__________     READ PMS7003     _________*/
void read_pms() {
  printLocalTime();
  t_hdc = hdc1080.readTemperature();
  h_hdc = hdc1080.readHumidity();
  int hdc_try = 0;
  while (t_hdc <0 || t_hdc >80){
    delay(3000);
    t_hdc = hdc1080.readTemperature();
    hdc_try +=1;
    if (hdc_try >=3){
      ESP.restart();
    }
  }
  
//  flush any data in buffer
  while(Serial.available() > 0) {
    char t = Serial.read();
  }

  pms.write(Pmsx003::cmdReadData);
  
  delay(1000);
  const auto n = Pmsx003::Reserved;
  Pmsx003::pmsData data[n];
  Pmsx003::PmsStatus status = pms.read(data, n);

  switch (status) {
    case Pmsx003::OK:{
      pm1_0_ac = data[3];
      pm2_5_ac = data[4];
      pm10_ac = data[5];
      um03 = data[6];
      um05 = data[7];
      um1 = data[8];
      um2_5 = data[9];
      um5 = data[10];
      um10 = data[11];
      lastSampling = round(millis()/1000);
      push_data();
      break;
    }
    case Pmsx003::noData:
      break;
    default:
      Serial.println("_________________");
      Serial.println(Pmsx003::errorMsg[status]);
      pm2_5_ac = 0;
      pm10_ac = 0;
  };
  // update OLED display
  display_main();
}

/*__________     DISPLAY TEXT     _________*/
void display_data(String input){
  oled.clear();
  oled.setColor(WHITE);
  oled.drawRect(0,0,128,64);
  oled.setFont(ArialMT_Plain_24);
  oled.setTextAlignment(TEXT_ALIGN_CENTER);
  oled.drawString(64, 2, String(input));
  oled.setFont(ArialMT_Plain_16);
  oled.drawString(64, 28, DATE);
  oled.drawString(64, 44, ip);
  oled.display();
}

void display_main(){
  int t_ = round(t_hdc);
  int h_ = round(h_hdc);
  
  oled.clear();
  oled.drawRect(0,0,128,64);
  oled.setTextAlignment(TEXT_ALIGN_CENTER);
  oled.setFont(ArialMT_Plain_10);
  oled.drawString(32, 30, "PM2.5");
  oled.drawString(98, 30, "PM10");
  
  oled.setFont(ArialMT_Plain_24);
  oled.drawString(32, 38, String(pm2_5_ac));
  oled.drawString(98, 38, String(pm10_ac));
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(1, 1, hm);
  oled.setFont(ArialMT_Plain_16);
  char th[9];
  sprintf(th, "%2d*%4d%%", t_, h_);
  oled.setTextAlignment(TEXT_ALIGN_RIGHT);
  oled.drawString(126, 4, th);
  oled.display();
  
}


/*______________    CUSTOMIZE HOST NAME       _______________*/
void create_host_name(){
  char MAC[6];
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  sprintf(MAC, "%02x%02x%02x", macAddr[3], macAddr[4], macAddr[5]);
//  Serial.println(MAC);
  HOSTNAME = String(SENSORNAME) + "-" + String(MAC);
  HOSTNAME.trim();
}


/*______________        _ SETUP WIFI _        _______________*/
void setup_wifi() {
  delay(10);
  Serial.printf("Connecting to %s", wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  delay(100); 
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    i++;
    Serial.printf(" %i ", i);
    if (i == 5){
      WiFi.mode(WIFI_STA);
      WiFi.begin(wifi_ssid, wifi_password);
      delay(3000);
    }
    if (i >=10){
      ESP.restart();
      Serial.println("Resetting ESP");
    }
  }
  ip = WiFi.localIP().toString();
  Serial.printf("\nWiFi connected:\t");
  Serial.print(ip);
  Serial.printf("\tMAC: %s\n", WiFi.macAddress().c_str());
  create_host_name();
  Serial.print("Default hostname  ");
  Serial.print(WiFi.getHostname());
  int str_len = HOSTNAME.length() + 1; 
  char HOSTN[str_len];
  HOSTNAME.toCharArray(HOSTN, str_len);
  
  bool tmp = WiFi.setHostname(HOSTN);
  
  Serial.printf("\t>>Hostname:  ");
  Serial.print(WiFi.getHostname());
}

/*______________        _ START OTA _        _______________*/
void setup_OTA(){
  ArduinoOTA.setPort(OTAport);
  ArduinoOTA.setHostname(SENSORNAME);
  ArduinoOTA.setPassword((const char *)OTApassword);
  ArduinoOTA.onStart([]() {
    Serial.println("Starting");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

/*______________        _  START PROCESS JSON _        _______________*/
bool processJson(char* json) {
  
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return false;
  }
 return true;
}
/*______________        _START SEND STATE _        _______________*/
void compose_data() {
  DynamicJsonDocument doc(512);
  doc["sensor"] = SENSORNAME;
  doc["uptime"] = uptime;
  doc["t_hdc"] = t_hdc;
  doc["h_hdc"] = h_hdc;
  doc["pm1_0_ac"] = pm1_0_ac;
  doc["pm2_5_ac"]= pm2_5_ac;
  doc["pm10_ac"] = pm10_ac;
  doc["um03"] = um03;
  doc["um05"] = um05;
  doc["um1"] = um1;
  doc["um2_5"] = um2_5;
  doc["um5"] = um5;
  doc["um10"] = um10;
  doc["type"] = "json";
  
  size_t len = measureJson(doc)+ 1;
  char payloads[len];
  
  serializeJson(doc, payloads, sizeof(payloads));
  
  if (!client.connected()) {
    reconnect();
    delay(1000);
  }
  
  if (client.publish(publish_topic, payloads, false)){
    Serial.println("Success: " + String(payloads));
  } else {
    Serial.println("Failed to push: " + String(payloads));
  } 
}

/*______________Connect to MQTT_______________*/
void push_data(){
   
  if (client.connect(SENSORNAME, mqtt_user, mqtt_password)){
    compose_data();   
  } else {     //attempt to connect client to broker
    for (int _try=0; _try <3; _try ++){
      delay(100);
      if (WiFi.status() != WL_CONNECTED) {
        setup_wifi();
      };
      if (client.connect(SENSORNAME, mqtt_user, mqtt_password)){
        compose_data();
        break;
      };
      if (_try >=3){
        Serial.print("\n: Restarting the ESP");
        ESP.restart();
      }
    }
  }
 }  

/*______________        _ START CALLBACK _        _______________*/
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println(message);
  
  if (!processJson(message)) {
    return;
  };
}

/*______________     RECONNECTING MQTT      _______________*/
void reconnect() {
  // check and connect to the rounter
  while (WiFi.status() != WL_CONNECTED) {
    setup_wifi();  
  }
  // check and connect to the MQTT server
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(SENSORNAME, mqtt_user, mqtt_password)) {
      Serial.println("connected");     
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/*__________   getTime  ___________*/
void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
//  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  strftime(hm,7, "%H:%M", &timeinfo);
//  strftime(date_,10, "%d/%m", &timeinfo);
//  strftime(dofw,10, "%A", &timeinfo);
  
}
