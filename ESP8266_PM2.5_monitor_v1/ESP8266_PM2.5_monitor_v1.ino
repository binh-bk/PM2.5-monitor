/*
- Binh Nguyen,September 30, 2020
- Using NTP time for clock
- PMS7003 (Plantower) for PM2.5 sensor
- using resistors to measure voltage
*/

/*______________        _LIBRARIES FOR EACH SENSOR _        _______________*/
#include <Wire.h>    // I2C library
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <pms.h>
#include "SSD1306.h"

#define wifi_ssid "wifi_ssid"
#define wifi_password "password"
#define DATE "v2.Sep30"

// for NTP time
const char* ntpServer = "pool.ntp.org";
const long utcOffsetInSeconds = 7*3600; // for GMT+7 
//const int   daylightOffset_sec = 0; // apply to US
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, utcOffsetInSeconds);

Pmsx003 pms(D6, D5);
SSD1306  oled(0x3c, D2, D1);

WiFiClient espClient;

uint16_t INVL = 10; // sample every minute
uint8_t pm2_5_ac,pm10_ac;
int32_t lastSampling = 0;
uint32_t uptime;
uint16_t bat_adc;
float bat;
char hm[7];
String ip;  // to trace back your device

/*______________        _ START SETUP _        _______________*/
void setup() {
  Serial.begin(115200);
  Serial.println("\nStarting");
  Wire.begin(5,4); 
  oled.init(); 
  oled.flipScreenVertically();
  oled.setBrightness(100);
  pinMode(A0, INPUT);

  setup_wifi();
  timeClient.begin();
  
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
  uptime = round(millis()/1000L);
  if ((uptime -lastSampling) > INVL){
    printLocalTime();
    read_pms();
    read_bat();
    display_main();
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
      // only get concentration of PM2.5 and PM10
      pm2_5_ac = data[4];
      pm10_ac = data[5];
      lastSampling = uptime;
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
  display_main();
}

/*__________     DISPLAY TEXT     _________*/
void display_data(String input){
  oled.clear();
  oled.setColor(WHITE);
  oled.drawRect(0,0,128,32);
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_CENTER);
  oled.drawString(64, 0, String(input));
  oled.setFont(ArialMT_Plain_10);
  oled.drawString(24, 16, DATE);
  oled.drawString(90, 16, ip);
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
  oled.drawString(1, 0, "  ug/m3  ");
  oled.setFont(ArialMT_Plain_16);
  
//  String pm = "PM2.5 " + String(pm2_5_ac);
  oled.drawString(10, 15, String(pm2_5_ac));
  oled.display();
}

/*______________      SETUP WIFI      _______________*/
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
  display_data(ip);
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
    Serial.printf("%d value %d\t", i, val);
    delay(100);
  }
  bat_adc = round(bat_adc/3);
  bat = bat_adc*4.652/1000.0;
  Serial.printf("Battery ADC: %d\t V: %.2f\n", bat_adc, bat);
  
}
