/*
- Binh Nguyen,Oct 07, 2020
v1. - Using NTP time for clock
    - PMS7003 (Plantower) for PM2.5 sensor
    - using resistors to measure voltage
v1.1:
    - transfer data to MQTT server
*/

/*______________        _LIBRARIES FOR EACH SENSOR _        _______________*/
#include <Wire.h>    // I2C library
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <pms.h>
#include "SSD1306.h"

#define SENSORNAME "pms7003"
#define wifi_ssid "your_wifi"
#define wifi_password "wifi_password"
#define mqtt_user "mqtt_user" 
#define mqtt_password "mqtt_password"
#define mqtt_server "192.168.1.100"  // change this to fit yours

#define publish_topic "sensors/pms7003"
#define subcribe_set "sensors/pms7003/set"

#define DATE "v2.Sep30"
#define BST_PIN D7 //BOOST PIN to turn on 5V boost

// for NTP time
const char* ntpServer = "pool.ntp.org";
const long utcOffsetInSeconds = 7*3600; // for GMT+7 
//const int   daylightOffset_sec = 0; // apply to US
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, utcOffsetInSeconds);

Pmsx003 pms(D6, D5);
SSD1306  oled(0x3c, D2, D1);

WiFiClient espClient;
PubSubClient client(espClient);

uint16_t INVL = 30; // sample every minute
uint16_t pm1_0_ac, pm2_5_ac,pm10_ac;
uint16_t pm1_0, pm2_5,pm10;
uint32_t um03;
uint16_t um05, um1, um2_5, um5, um10;
int32_t lastSampling = 0;
uint32_t uptime;
uint16_t bat_adc;
float bat;
char hm[7];
String HOSTNAME;

/*______________        _ START SETUP _        _______________*/
void setup() {
  Serial.begin(115200);
  Serial.println("\nStarting");
  pinMode(BST_PIN, OUTPUT);
  digitalWrite(BST_PIN, LOW);
  Serial.println("Turn on 5V boost");
  delay(1000); // pull low for 500 to 2000 to turn on
  digitalWrite(BST_PIN, HIGH);
  delay(2000); // wait for boost online
  pinMode(A0, INPUT);
  
  Wire.begin(5,4); 
  oled.init(); 
  oled.flipScreenVertically();
  oled.setBrightness(100);
  oled.setColor(WHITE);
  oled.drawRect(0,0,128,32);
  oled.setFont(ArialMT_Plain_10);
  oled.drawString(24,1, "version");
  oled.drawString(24, 16, DATE);
  oled.display();
  

  setup_wifi();
  timeClient.begin();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
//   init PMS sensor
  pms.begin();
  pms.waitForData(Pmsx003::wakeupTime);
  pms.write(Pmsx003::cmdModePassive); // query mode, PMS sends data if requested by MCU

  // init temperature & RH sensor
  display_data("Air Monitior");
  Serial.println("LEAVING SETUP LOOP");
  delay(1000);
}

/*______________        _ START MAIN LOOP _        _______________*/
void loop() {
  client.loop(); //if you expect to keep the ESP8266 online 
  uptime = round(millis()/1000L);
  if ((uptime -lastSampling) > INVL){
    printLocalTime();
    read_pms();
    read_bat();
    display_main();
    compose_data();
//    lastSampling = uptime;
  }
  delay(1000);
}

/*__________     READ PMS7003     _________*/
void read_pms() {
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
      // unpack all data from PMS data stream
      pm1_0 = data[0];
      pm2_5 = data[1];
      pm10= data[2];
      pm1_0_ac = data[3];
      pm2_5_ac = data[4];
      pm10_ac = data[5];
      um03 = data[6];
      um05 = data[7];
      um1 = data[8];
      um2_5 = data[9];
      um5 = data[10];
      um10 = data[11];
      lastSampling = uptime;
      break;
    }
    case Pmsx003::noData:
      break;
    default:
      Serial.println("_________________");
      Serial.println(Pmsx003::errorMsg[status]);
      pm1_0 = 0; // two of these data to signal error
      um03 = 0;
  };
  display_main();
}

/*__________     DISPLAY TEXT     _________*/
void display_data(String input){
  oled.clear();
  oled.drawRect(0,0,128,32);
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_CENTER);
  oled.drawString(64, 0, String(input));
  oled.setFont(ArialMT_Plain_10);
  
  oled.display();
}

void display_main(){
 
  oled.clear();
  oled.drawRect(0,0,128,32);
  oled.setTextAlignment(TEXT_ALIGN_RIGHT);
  oled.setFont(ArialMT_Plain_10);
  oled.drawString(127, 0, hm);
  oled.drawString(127, 16, String(bat) + "v");
  
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(1, 0, "ug/m3");
  oled.setFont(ArialMT_Plain_24);
  
//  String pm = "PM2.5 " + String(pm2_5_ac);
  oled.drawString(35, 0, String(pm2_5_ac));
  oled.display();
}

/*______________      SETUP WIFI      _______________*/
void setup_wifi() {
  delay(10);
  Serial.printf("\nConnecting to %s", wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  delay(1000); 
  int i = 0;
  int status_ = WiFi.status();
  while (status_ != 6) { //connected
    
    delay(1000);
    i++;
    Serial.printf(" %i ", i);
    if ((i%3)==0){
      WiFi.mode(WIFI_STA);
      WiFi.begin(wifi_ssid, wifi_password);
      
      Serial.printf("\nAttempt at %d\n", i);
      delay(5000);
      status_ = WiFi.status();
      Serial.printf("\nWiFi status: %i\n",status_);
    }
    String tmp = "Connecting..." + String(i);
    display_data(tmp);
    if (i >=20){
//      break;  // break out the while loop
      Serial.println("Resetting ESP");
      ESP.restart();
      
    
    }
  }
  String ip;
  if (WiFi.status() == WL_CONNECTED){
    ip = WiFi.localIP().toString();
    ip = "Connected: \n" + ip;
    display_data(ip);
    Serial.printf("\nWiFi connected:\t");
    Serial.print(ip);
    Serial.printf("\tMAC: %s\n", WiFi.macAddress().c_str()); 
    
  } else {
    ip = "No internet connection\nNTP time not available";
    display_data(ip);
  }
}

/*__________   getTime  ___________*/
void printLocalTime(){
  timeClient.update();
  sprintf(hm, "%2d:%02d", timeClient.getHours(), timeClient.getMinutes());
//  Serial.printf("Time is %s\n", hm);
}

/*__________   read Battery  ___________*/
void read_bat(){
  bat_adc = 0;
  for (int i=0; i<3; i++){
    int val = analogRead(A0);
    bat_adc += val;
//    Serial.printf("%d value %d\t", i, val);
    delay(100);
  }
  bat_adc = round(bat_adc/3);
  bat = bat_adc*4.652/1000.0;
  Serial.printf("Battery ADC: %d\t V: %.2f\n", bat_adc, bat);
}
/*______________    convert string to json      _______________*/
bool processJson(char* json) {
  // this is an exemple to set new interval via subscribe method
  
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return false;
  }
  if (doc.containsKey("invl")) {
    if (doc["sensor"] == SENSORNAME){
      int tmp = doc["invl"].as<int>();
      if ((tmp>=10) or (tmp <=3600)){
        INVL = tmp; // setting new interval via subscribe
        Serial.printf("Set Interval:\t%d", INVL);
        }
    }
  }
 return true;
}

/*______________      compose data       _______________*/
void compose_data() {
  DynamicJsonDocument doc(512);
  doc["sensor"] = SENSORNAME;
  doc["uptime"] = uptime;
  doc["pm1_0"] = pm1_0;
  doc["pm2_5"]= pm2_5;
  doc["pm10"] = pm10;
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

/*______________   callback functiion      _______________*/
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

/*______________     reconnecting MQTT    _______________*/
void reconnect() {
  // check and connect to the rounter
  while (WiFi.status() != WL_CONNECTED) {
    setup_wifi();  
  }
  // check and connect to the MQTT server
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(SENSORNAME, mqtt_user, mqtt_password)) {
//    if (client.connect(SENSORNAME)) {
      Serial.println("connected");
      client.subscribe(subcribe_set);
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
