import requests
import json
from function import bcolors

uid = "NATFcsIhL4RPfh3Zf6iJDMhDUrG3"
uname = "farm4"

def post(uid, uname, data):
    url = "https://smart-kits.tech/update_value"
    header = {"Content-Type": "application/json",
              "uid": uid, "uname": uname}
    r = requests.post(url, headers=header, data=data)
    if "200" in str(r):
        print(" ├" + bcolors.OKGREEN +
              " Data value updated. " + str(r) + bcolors.ENDC)
    else:
        print(" ├" + bcolors.FAIL +
              " Data value updated. " + str(r) + bcolors.ENDC)


def get_response(uid):
    url = "https://smart-kits.tech/auto/get_automation"
    header = {"Content-Type": "application/json", "uid": uid}
    response = requests.get(url, headers=header)
    if "200" in str(response):
        return response.json()
    else:
        print(bcolors.FAIL + bcolors.BOLD+"\n"+str(response) +
              " Eror response from get_response function."+bcolors.ENDC)
        return None


def post_chart(uid, uname, data):
    url = "https://smart-kits.tech/update_value/chart"
    header = {"Content-Type": "application/json", "uid": uid}
    r = requests.post(url, headers=header, data=data)
    if "200" in str(r):
        print(" ├" + bcolors.OKGREEN +
              " Data chart updated. " + str(r) + bcolors.ENDC)
    else:
        print(" ├" + bcolors.FAIL +
              " Data chart updated. " + str(r) + bcolors.ENDC)


def get_devicename(uid, uname, devicename):
    url = "https://smart-kits.tech/uname"
    header = {"Content-Type": "application/json",
              "uid": uid, "uname": uname, "devicename": devicename}
    response = requests.get(url, headers=header)
    print(bcolors.OKCYAN + bcolors.BOLD+"\nThis is new Device, Adding new data."+bcolors.ENDC) if response.json(
    ) else print(bcolors.OKBLUE + bcolors.BOLD+"This is my device, Don't add new data."+bcolors.ENDC)
    if "200" in str(response):
        return response.json()
    else:
        print(bcolors.FAIL + bcolors.BOLD+"\n"+str(response) +
              " Eror response from get_devicename function."+bcolors.ENDC)
        return False
