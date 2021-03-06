# PM2.5-monitor
Build air quality monitor of PM2.5 (fine particles) with low-cost sensor and ESP32/ESP8266

<p align="center">
  <img src="img/pm25.gif"/>
</p>

## v1. Easy - peasy
### Schematics:
<p align="center">
  <img src="img/pm25_v1.jpg"/>
</p>

### Inside:
<p align="center">
  <img src="img/inside.jpg"/>
</p>

### Parts:  
1. PMS7003 (my choice), also works with PMS5003. Other dust sensor works as well, subject to change of the code  
2. ESP8266 (WeMos D1 mini)  
3. Lithium battery protection + boost-up board (5V)  
4. OLED display  

## v1.1 Logging data via MQTT

### Publish data to open MQTT server - [HiveMQ.com](https://b-io.info/tutorials/pm25-monitor/4/)

### Publish data to a home MQTT server
- Capture data by a [Python script](https://github.com/binh-bk/PM2.5-monitor/blob/master/py/collector.py)
- Convert data in a [list of dictionary to a CSV file](https://github.com/binh-bk/PM2.5-monitor/blob/master/py/convert2csv.py)

### Tutorials:
See it on my personal website [b-io.info](https://www.b-io.info/post/tutorial/pm25-monitor/)

