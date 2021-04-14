from function import get_date, get_locationOfSlash, get_time, get_hours
import paho.mqtt.client as mqtt
import time
import threading
import json
import request
import requests

host = "pigateway.sytes.net"
port = 1883
uid = "NATFcsIhL4RPfh3Zf6iJDMhDUrG3"
unique_name = "farm4"

switcher = {
    "switch1": "D2",
    "switch2": "D5",
    "pwm": "D6"
}

response = {}
payload, node_list = [], []
last_time, c, last_hours = 0, 0, get_hours()

client = mqtt.Client()


def on_connect(client, userdata, flags, rc):
    print("Connected success") if rc == 0 else print(
        f"Connected fail with code {rc}")
    client.subscribe("#")


def on_message(client, userdata, message):
    topic = message.topic
    message = str(message.payload.decode("utf-8"))
    storeSensorValue(topic, message)
    automationOnlyM1(topic, message)


def storeSensorValue(topic, message):
    global node_list, payload
    location = get_locationOfSlash(topic)
    if(len(location) > 2 and ((topic[location[2]+1:location[3]]) == "sensor")
       and (topic[location[0]:location[1]]) == unique_name):

        node = (topic[location[1]+1:location[2]])
        temperature = (topic[location[3]+1:location[4]])
        humidity = (topic[location[4]+1:location[5]])
        analog = (topic[location[5]+1:])

        if("device_added" in message):
            if(request.get_devicename(uid=uid, uname=unique_name, devicename=node)):
                new_device = [{
                    "devicename": node,
                    "temperature": temperature,
                    "humidity": humidity,
                    "analog": analog
                }]
                request.post(uid=uid, uname=unique_name,
                             data=json.dumps(new_device))

        elif node not in node_list:
            node_list.append(node)
            payload.append({
                "devicename": node,
                "temperature": temperature,
                "humidity": humidity,
                "analog": analog
            })


def runSubscribe():
    while(True):
        client.on_connect = on_connect
        client.on_message = on_message
        client.connect(host, port)
        client.loop_forever()
        client.disconnect()
        time.sleep(1)


def runEveryMinute():
    global c, response, payload, node_list, last_time, last_hours
    while(True):
        if get_date("minute") != last_time:
            last_time = get_date("minute")
            response = request.get_response(uid=uid)

            print("")
            print("Time "+str(last_hours) + ":" + str(last_time)+" O'clock")

            if(get_hours() != last_hours and payload != []):
                request.post_chart(uid=uid, uname=unique_name, data=payload)
                last_hours = get_hours()

            if(c > 0 and payload != []):
                request.post(uid=uid, uname=unique_name,
                             data=json.dumps(payload))
                payload, node_list = [], []
            else:
                c = c+1

            automationM0_M2()
        time.sleep(1)


def automationM0_M2():
    if response:
        for i in response:
            if(i["uniqueName"] == unique_name):
                node = i["devicename"]
                i = (i["automations"])
                for j in i:
                    if j["mode"] == "กำหนดเอง":
                        command = j["action"]
                    elif j["mode"] == "ตั้งเวลา":
                        start = int(j["str_time"].replace(":", ""))
                        end = int(j["end_time"].replace(":", ""))
                        now = get_time()

                        if start > end:
                            if now < start and now <= end:
                                now += 10000
                            end += 10000

                        if (start <= now < end):
                            command = j["action"]
                        elif j["action"] == "true":
                            command = "false"
                        elif j["action"] == "false":
                            command = "true"
                    else:
                        continue
                    client.publish(unique_name + "/" + node + "/control/" +
                                switcher.get(j["module"]), payload=command, qos=1)

                print("Updated nodes status of : "+node)


def automationOnlyM1(topic, message):
    location = get_locationOfSlash(topic)
    if(len(location) > 2 and ((topic[location[2]+1:location[3]]) == "sensor")
       and (topic[location[0]:location[1]]) == unique_name and message != "device_added"):

        node = (topic[location[1]+1:location[2]])
        temperature = (topic[location[3]+1:location[4]])
        humidity = (topic[location[4]+1:location[5]])
        analog = (topic[location[5]+1:])

        if response:
            for i in response:
                if(i["uniqueName"] == unique_name):
                    thisnode = i["devicename"]
                    i = i["automations"]
                    for j in i:
                        if j["mode"] == "อัตโนมัติ" and j["node_target"] == node:

                            if("off" in temperature or "off" in humidity or "off" in analog):
                                continue

                            elif(j["sensor_target"] == "temperature"):
                                sensorValue = float(temperature)
                            elif (j["sensor_target"] == "humidity"):
                                sensorValue = float(humidity)
                            else:
                                sensorValue = float(analog)

                            value = float(j["value"])

                            if (j["operator"] == "มากกว่า"):
                                if (j["action"] == "true"):
                                    command = "true" if (
                                        sensorValue > value) else "false"
                                elif (j["action"] == "false"):
                                    command = "false" if (
                                        sensorValue > value) else "true"
                                else:
                                    command = j["action"]

                            elif (j["operator"] == "น้อยกว่า"):
                                if (j["action"] == "true"):
                                    command = "true" if (
                                        sensorValue < value) else "false"
                                elif (j["action"] == "false"):
                                    command = "false" if (
                                        sensorValue < value) else "true"
                                else:
                                    command = j["action"]

                            elif (j["operator"] == "เท่ากับ"):
                                if (j["action"] == "true"):
                                    command = "true" if (
                                        sensorValue == value) else "false"
                                elif (j["action"] == "false"):
                                    command = "false" if (
                                        sensorValue == value) else "true"
                                else:
                                    command = j["action"]

                            client.publish(unique_name + "/" + thisnode + "/control/" +
                                           switcher.get(j["module"]), payload=command, qos=1)


if __name__ == "__main__":
    t1 = threading.Thread(target=runSubscribe)
    t2 = threading.Thread(target=runEveryMinute)
    try:
        t1.start()
        t2.start()
    except:
        print("Something went wrong")
