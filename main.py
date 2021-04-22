from function import get_date, get_locationOfSlash, get_time, get_hours, bcolors
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

auto_node, auto_module = [], []
response = {}
payload, node_list = [], []
last_sec, last_minute, c, last_hours = 0, 0, 0, get_hours()


client = mqtt.Client()


def on_connect(client, userdata, flags, rc):
    client.subscribe("#")
    time.sleep(0.1)
    print(" ├ " + bcolors.OKGREEN + f"Connected success : {host}" + bcolors.ENDC) if rc == 0 else print(
        " ├ " + bcolors.FAIL + f"Connected fail with code {rc}" + bcolors.ENDC)
    

def on_message(client, userdata, message):
    topic = message.topic
    message = str(message.payload.decode("utf-8"))
    storeSensorValue(topic, message)
    automationOnlyM1(topic, message)


def storeSensorValue(topic, message):
    global node_list, payload, response
    location = get_locationOfSlash(topic)
    if(len(location) > 2 and ((topic[location[2]+1:location[3]]) == "sensor")
       and (topic[location[0]:location[1]]) == unique_name):

        node = (topic[location[1]+1:location[2]])
        temperature = (topic[location[3]+1:location[4]])
        humidity = (topic[location[4]+1:location[5]])
        analog = (topic[location[5]+1:])

        if("trig_to_update" in message):
            response = request.get_response(uid=uid)
            print(bcolors.BOLD + bcolors.OKGREEN +
                  "\nYou setting is Trig to updated." + bcolors.ENDC)
            automationM0_M2()

        elif("device_added" in message):
            print(bcolors.BOLD + bcolors.WARNING +
                  "Device detect." + bcolors.ENDC)
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
    global c, response, payload, node_list, last_minute, last_hours, last_sec
    strMinute, strHours = "", "0"+str(last_hours) if last_hours<10 else str(last_hours)
    while(True):
        try:
            time.sleep(0.01)
            microsecond = int(get_date("milli")/1000)
            milli = "0" + str(microsecond) if microsecond < 100 else str(microsecond)
            if microsecond < 10:
                milli = "0" + milli 

            second = get_date("second")
            strSecond = ("0"+str(second)) if second < 10 else str(second)
            # if second != last_sec and second != 0:
            #     last_sec = get_date("second")
            print(
                f"{bcolors.WARNING}{bcolors.BOLD}Time {strHours}:{strMinute}:{strSecond}:{milli} O'colck{bcolors.ENDC}", end="\r")
            if get_date("minute") != last_minute:
                last_minute = get_date("minute")
                strMinute = "0"+str(last_minute) if last_minute<10 else str(last_minute)
                print(f"\r{bcolors.WARNING}{bcolors.BOLD}Time {strHours}:{strMinute}:{strSecond}:{milli} O'colck{bcolors.ENDC}")
                buff = request.get_response(uid=uid)
                if response != buff and buff != None:
                    response = buff
                    print(" ├ " + bcolors.OKGREEN +
                          "You setting is updated." + bcolors.ENDC)

                if(get_hours() != last_hours):
                    request.post_chart(
                        uid=uid, uname=unique_name, data=json.dumps(payload)) if payload != [] else print(" ├ "+bcolors.WARNING +
                                                                                                          "Payload is empty, do not upload chart."+bcolors.ENDC)
                    last_hours = get_hours()
                    strHours = "0"+str(last_hours) if last_hours<10 else str(last_hours)

                if(c > 0):
                    if payload != []:
                        request.post(uid=uid, uname=unique_name,
                                     data=json.dumps(payload))
                        payload, node_list = [], []
                    else:
                        print(" ├ "+bcolors.WARNING +
                              "Payload is empty, do not upload value."+bcolors.ENDC)
                else:
                    c = c+1

                print(" ├ " + bcolors.OKCYAN + "Automations is runnig for : " +
                      str(len(auto_node))+" node "+str(len(auto_module))+" module" + bcolors.ENDC)
                automationM0_M2()

        except KeyboardInterrupt:
            raise
        except:
            print("Exception in EverySecond")


def automationM0_M2():
    if response:
        nodeBuff = []
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
                nodeBuff.append(node)

        print(" └ " + bcolors.OKCYAN + "Updating nodes status for : " +
              str(nodeBuff).replace("[", "").replace("]", "").replace("'", "") + "\n" + bcolors.ENDC)


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

                            if (thisnode+":"+j["module"]) not in auto_module:
                                auto_module.append(
                                    str(thisnode+":"+j["module"]))
                            if thisnode not in auto_node:
                                auto_node.append(thisnode)


if __name__ == "__main__":
    t1 = threading.Thread(target=runSubscribe)
    t2 = threading.Thread(target=runEveryMinute)
    t1.start()
    t2.start()
