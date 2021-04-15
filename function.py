from datetime import datetime
import time
import pytz


class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def get_locationOfSlash(text):
    count = 0
    location = [0]
    for i in text:
        if (i == "/"):
            location.append(count)
        count = count+1
    return location


def get_date(setting):
    time_zone = pytz.timezone("Asia/Bangkok")
    if setting == "stamp":
        time_stamp = datetime.timestamp(datetime.now(time_zone))
        return time_stamp
    elif setting == "minute":
        time = datetime.now(time_zone)
        return time.minute
    elif setting == "second":
        time = datetime.now(time_zone)
        return time.second

def get_time():
    return int(datetime.now().strftime("%H%M"))

def get_hours():
    return int(datetime.now().strftime("%H"))
