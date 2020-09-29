/*
- Binh Nguyen,September 30, 2020
- Using NTP time for clock
- PMS7003 (Plantower) for PM2.5 sensor
- HDC1080 (TI) for temperature and relative humidity
*/

/*______________        _LIBRARIES FOR EACH SENSOR _        _______________*/
#include <Wire.h>    // I2C library
#include <WiFi.h>
#include <pms.h>
#include "SSD1306.h"
#include "ClosedCube_HDC1080.h"

#define wifi_ssid "your_wifi_name"
#define wifi_password "your_wifi_password"
#define DATE "v2.Sep30"

// for NTP time
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7*3600; // for GMT+7 
const int   daylightOffset_sec = 0; // apply to US

Pmsx003 pms(13, 15);
SSD1306  oled(0x3c, 5, 4);

WiFiClient espClient;
ClosedCube_HDC1080 hdc1080;

uint16_t INVL = 60; // sample every minute
uint8_t pm2_5_ac,pm10_ac;
int32_t lastSampling = 0;
uint32_t uptime;
char hm[7];
float t_hdc, h_hdc;
String ip;  // to trace back your device


/*______________        _ START SETUP _        _______________*/
void setup() {
  Serial.begin(115200);
  Serial.println("\nStarting");
  Wire.begin(5,4); 
  oled.init();
  oled.flipScreenVertically();

  setup_wifi();
  
  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();

  // init PMS sensor
  pms.begin();
  pms.waitForData(Pmsx003::wakeupTime);
  pms.write(Pmsx003::cmdModePassive); // query mode, PMS sends data if requested by MCU

  // init temperature & RH sensor
  hdc1080.begin(0x40);
  display_data("Air Monitior");
  Serial.println("LEAVING SETUP LOOP");
  delay(1000);
}

/*______________        _ START MAIN LOOP _        _______________*/
void loop() {
  uptime = round(millis()/1000L);
  if ((uptime%3600) < 10){
    // update NTP time every hour
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  }
  
  if ((uptime -lastSampling) > INVL){
    read_pms();
  }
  delay(1000);
}

/*__________     READ PMS7003     _________*/
void read_pms() {
  printLocalTime();  //update time 

  t_hdc = hdc1080.readTemperature();
  h_hdc = hdc1080.readHumidity();
  int hdc_try = 0;
  while (t_hdc <0 || t_hdc >80){
    delay(2000);
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
      // only get concentration of PM2.5 and PM10
      pm2_5_ac = data[4];
      pm10_ac = data[5];
      lastSampling = round(millis()/1000);
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
  Serial.printf("\t>>Hostname:  ");
  Serial.print(WiFi.getHostname());
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
}
