import requests
import json
from function import bcolors

uid = "NATFcsIhL4RPfh3Zf6iJDMhDUrG3"
uname = "farm4"


def post(uid, uname, data):
    count, state = 1, True
    while state:
        url = "https://smart-kits.tech/update_value"
        header = {"Content-Type": "application/json",
                  "uid": uid, "uname": uname}
        r = requests.post(url, headers=header, data=data)
        if "200" in str(r):
            print(" ├" + bcolors.OKGREEN +
                  " Data value updated. " + str(r) + bcolors.ENDC)
            state = False
        elif count <= 5:
            print(" ├" + bcolors.FAIL +
                  f" Data value update retry #{count}. " + str(r) + bcolors.ENDC)
            count = count + 1
        else:
            print(" ├" + bcolors.FAIL +
                  " Data value update time out. " + str(r) + bcolors.ENDC)
            break


def get_response(uid):
    count, state = 1, True
    while state:
        url = "https://smart-kits.tech/auto/get_automation"
        header = {"Content-Type": "application/json", "uid": uid}
        response = requests.get(url, headers=header)
        if "200" in str(response):
            return response.json()
            state = False
        elif count <= 5:
            print(" ├ "+bcolors.FAIL + bcolors.BOLD +
                  f"Eror response from get_response retry #{count}. "+str(response) + bcolors.ENDC)
            count = count + 1
        else:
            print(" ├ "+bcolors.FAIL + bcolors.BOLD +
                  f"Eror response from get_response time out. "+str(response) + bcolors.ENDC)
            return None


def post_chart(uid, uname, data):
    count, state = 1, True
    while state:
        url = "https://smart-kits.tech/update_value/chart"
        header = {"Content-Type": "application/json", "uid": uid}
        r = requests.post(url, headers=header, data=data)
        if "200" in str(r):
            print(" ├" + bcolors.OKGREEN +
                  " Data chart updated. " + str(r) + bcolors.ENDC)
            state = False
        elif count <= 5:
            print(" ├" + bcolors.FAIL +
                  f" Data chart update retry #{count}. " + str(r) + bcolors.ENDC)
            count = count + 1
        else:
            print(" ├" + bcolors.FAIL +
                  " Data chart update time out. " + str(r) + bcolors.ENDC)
            break


def get_devicename(uid, uname, devicename):
    count, state = 1, True
    while state:
        url = "https://smart-kits.tech/uname"
        header = {"Content-Type": "application/json",
                  "uid": uid, "uname": uname, "devicename": devicename}
        response = requests.get(url, headers=header)
        if "200" in str(response):
            print(bcolors.OKCYAN + bcolors.BOLD+"\nThis is new Device, Adding new data."+bcolors.ENDC) if response.json(
            ) else print(bcolors.OKBLUE + bcolors.BOLD+"This is my device, Don't add new data."+bcolors.ENDC)
            state = False
            return response.json()
        elif count <= 5:
            print(" ├ " + bcolors.FAIL + bcolors.BOLD +
                  f"Eror response from get_devicename retry #{count}. "+str(response) + bcolors.ENDC)
            count = count + 1
        else:
            print(" ├ " + bcolors.FAIL + bcolors.BOLD + str(response) +
                  " Eror response from get_devicename time out. " + bcolors.ENDC)
            return False

def notifications(uid, data):
    count, state = 1, True
    while state:
        url = "https://smart-kits.tech/notifications"
        header = {"Content-Type": "application/json", "uid": uid}
        r = requests.post(url, headers=header, data=data)
        if "200" in str(r):
            print(" ├" + bcolors.OKGREEN +
                  " Notification send. " + str(r) + bcolors.ENDC)
            state = False
        elif count <= 5:
            print(" ├" + bcolors.FAIL +
                  f" Notification send retry #{count}. " + str(r) + bcolors.ENDC)
            count = count + 1
        else:
            print(" ├" + bcolors.FAIL +
                  " Notification send time out. " + str(r) + bcolors.ENDC)
            break
