#! /usr/bin/python3
# Binh Nguyen, Oct 2002
# listen to MQTT server, listen to a topic, and log data in a list of dictionary

import time
import json
import os
import paho.mqtt.client as mqtt
import paho.mqtt.publish as publish


basedir = os.path.abspath(os.path.dirname(__file__))
logFile = os.path.join(basedir, 'logs', '{}.log'.format(__file__))

# DATABASE
auth = {
    'topic': "sensors/pms7003",
    'password': 'mqtt_password',
    'user': 'mqtt_user', 
    'ip': '192.168.1.100'
}

def to_timestamp():
    return time.strftime("%Y%m%d_%H%M%S")
    
start_time = to_timestamp()  # mark the timestamp for recording

# SHORTCUTS

def host_folder():
    '''designate a folder for each money, create one if none existed'''
    this_month_folder = time.strftime('%b%Y')
    all_dirs = [d for d in os.listdir(basedir) if os.path.isdir(os.path.join(basedir,d))]
    if len(all_dirs) == 0 or this_month_folder not in all_dirs:
        print('created: {}'.format(this_month_folder))
        os.mkdir(os.path.join(basedir,this_month_folder))
    return os.path.join(basedir, this_month_folder)
    

def takeTime():
    return time.strftime("%Y-%m-%d %H:%M:%S")

def get_logfile(payload):
    '''create log file name based on sensor'''
    file = f'{payload["sensor"]}_{start_time}.txt'
    file = os.path.join(host_folder(), file)
    # files.update({sensor:file})
    return file

def record(payload):
    record_file = get_logfile(payload)    
    with open(record_file, 'a+') as f:
        f.write(json.dumps(payload))
        f.write('\n')
        print(f'saved: {payload}')
    return None
    

def on_connect(client, userdata, flags, rc):
    client.subscribe(auth['topic'])
    print("Connected with result code "+str(rc))
    return None


def on_message(client, userdata, msg):
    # try:
    # topic = msg.topic
    payload = msg.payload.decode('UTF-8').lower().strip() 
    print(payload)  
    if msg.retain == 0:
        if "json" in payload:
            payload = json.loads(payload)
            payload['time'] = takeTime()
            record(payload)
    return None


def on_disconnect(client, userdata, rc):
    if rc != 0:
        print("Unexpected disconnection!")
    else:
        print("Disconnecting")
    return None


# Program starts here
client = mqtt.Client(client_id='py-collector')
client.username_pw_set(username=auth['user'], password=auth['password'])
client.connect(auth['ip'], 1883, 60)
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect
time.sleep(1)
client.loop_forever()
