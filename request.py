import requests
import json


def post(uid, uname, data):
    url = "https://us-central1-smartfarmkits.cloudfunctions.net/explore/update_value"
    header = {"Content-Type": "application/json",
              "uid": uid, "uname": uname}
    r = requests.post(url, headers=header, data=data)
    print(str(r)+" Data value updated... ")
    print(data)


def get_response(uid):
    url = "https://us-central1-smartfarmkits.cloudfunctions.net/explore/auto/get_automation"
    header = {"Content-Type": "application/json", "uid": uid}
    response = requests.get(url, headers=header)
    print("")
    if "200" in str(response):
        return response.json()
    else:
        print(response)
        print(response.json())
        return None


def post_chart(uid, uname, data):
    url = "https://us-central1-smartfarmkits.cloudfunctions.net/explore/update_value/chart"
    header = {"Content-Type": "application/json", "uid": uid}
    print(str((requests.post(url, headers=header, data=json.dumps(data)))
              )+" Data chart updated... ")


def get_devicename(uid, uname, devicename):
    url = "https://us-central1-smartfarmkits.cloudfunctions.net/explore/uname"
    header = {"Content-Type": "application/json",
              "uid": uid, "uname": uname, "devicename": devicename}
    response = requests.get(url, headers=header)
    print("This is new Device, Adding new data.") if response.json(
    ) else print("This is my device, Don't add new data.")
    if "200" in str(response):
        return response.json()
    else:
        print("error devicename")
        return False
