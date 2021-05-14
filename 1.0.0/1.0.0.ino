
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <cstdlib>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <Arduino.h>
#include <Arduino_JSON.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266HTTPClient.h>
#include "DHTesp.h"

AsyncWebServer server(80);

String ssid, password, device_name, user_id;

const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "password";
const char *PARAM_INPUT_3 = "deviceName"; //deviceName to device_name
const char *PARAM_INPUT_4 = "uName";      //uName to user_id
//const char* serverName = "https://us-central1-smartfarmkits.cloudfunctions.net/explore/notifications";

#define mqtt_server "pigateway.sytes.net"
#define mqtt_port 1883
#define relay1_pin 4
#define relay2_pin 14
#define dht_pin 5
#define pwm_pin 12
#define aStatus_pin 16

bool dht_status = false; // Temp Status value
bool analog_status = false;
bool data_status = false;
bool start_device = true;

int timezone = 7 * 3600; // Rael Time Clock
int dst = 0;
int count = 0; // For check only one time to PUBLIC
int wificounter = 30;
int wifireconne = 0;
int rand_period = 0;

DHTesp dht;
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
unsigned long lastMil = 0;

boolean inword(char str[], char swrd[]);
void popupLoginPage();
void AP_Mode();
void callback(char *topic, byte *payload, unsigned int length);

void setup()
{
  Serial.begin(115200);
  pinMode(aStatus_pin, INPUT);
  pinMode(relay1_pin, OUTPUT);       //D2 FOR RELAY CHANNEL1
  pinMode(relay2_pin, OUTPUT);       //D5 FOR RELAY CHANNEL2
  pinMode(pwm_pin, OUTPUT);          //D6 FOR PWM
  digitalWrite(relay1_pin, HIGH);    //SET RELAY OFF
  digitalWrite(relay2_pin, HIGH);    //SET RELAY OFF
  dht.setup(dht_pin, DHTesp::DHT11); //D1PIN FOR DHT11

  EEPROM.begin(512); //EEPROM Setting for 512 byte and Read EEPROM
  ssid = read_EEPROM(0);
  password = read_EEPROM(sizeof(ssid) + 1);
  user_id = read_EEPROM(sizeof(ssid) + sizeof(password) + 1);
  device_name = read_EEPROM(sizeof(ssid) + sizeof(password) + sizeof(user_id) + 1);

  if (ssid != NULL && password != NULL && user_id != NULL && device_name != NULL)
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("\n");
    while ((WiFi.status() != WL_CONNECTED) && (wificounter >= 0))
    {
      delay(1000);
      Serial.print("Reconnecting and Reboot in ");
      Serial.print(wificounter);
      Serial.println(" second.");
      wificounter--;
    }
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.print("\nWiFi connected : ");
      Serial.println(WiFi.localIP());
    }
    else
    {
      Serial.println("\nWiFi fail connected");
      Serial.println("clear_EEPROM");
      clear_EEPROM();
      ESP.restart();
    }
  }
  else
  {
    AP_Mode();
  }
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  configTime(timezone, dst, "pool.ntp.org", "time.nist.gov");
  while (!time(nullptr))
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Real-Time's ready, Loop run...");
}

//-----------------------------< EEPROM Function >-----------------------------------------------------------------------------------------------------------------------------------------------------//
void clear_EEPROM()
{
  for (int i = 0; i < EEPROM.length(); i++)
  {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}

void write_EEPROM(char add, String data)
{
  int i, _size = data.length();
  for (i = 0; i < _size; i++)
  {
    EEPROM.write(add + i, data[i]);
  }
  EEPROM.write(add + _size, '\0');
  EEPROM.commit();
}

String read_EEPROM(char add)
{
  int i, len = 0;
  char data[64];
  unsigned char k;
  k = EEPROM.read(add);
  while (k != '\0' && len < 500)
  {
    k = EEPROM.read(add + len);
    data[len] = k;
    len++;
  }
  data[len] = '\0';
  return String(data);
}
//-----------------------------< Publish Function >-----------------------------------------------------------------------------------------------------------------------------------------------------//

void public_message(String topic, String msg_) //Set String to char[] in length.
{
  char buf[msg_.length() + 1];
  char buf2[topic.length() + 1];
  msg_.toCharArray(buf, msg_.length() + 1);
  topic.toCharArray(buf2, topic.length() + 1);
  client.publish(buf2, buf);
}

String get_temperature()
{
  float t = dht.getTemperature();
  if (dht.getStatusString() == "OK")
  {
    char temperature[16] = {};
    snprintf(temperature, sizeof(temperature), "%.2f", t);
    return temperature;
  }
  else
    return "disable";
}

String get_humidity()
{
  float h = dht.getHumidity();
  if (dht.getStatusString() == "OK")
  {
    char humidity[16] = {};
    snprintf(humidity, sizeof(humidity), "%.2f", h);
    return humidity;
  }
  else
    return "disable";
}

String get_analog()
{
  digitalRead(aStatus_pin);
  if (digitalRead(aStatus_pin))
  {
    float l = map(analogRead(A0), 0, 1023, 1023, 0);
    char analog[16] = {};
    snprintf(analog, sizeof(analog), "%.2f", l);
    return analog;
  }
  else
    return "disable";
}

String set_topic()
{
  String topic_all;
  topic_all += user_id;
  topic_all += "/";
  topic_all += device_name;
  topic_all += "/sensor/";
  topic_all += String(get_temperature());
  topic_all += "/";
  topic_all += String(get_humidity());
  topic_all += "/";
  topic_all += String(get_analog());

  return topic_all;
}

//-----------------------------< Subscribe Function >---------------------------------------------------------------------------------------------------------------------------------------------------//

void callback(char *topic, byte *payload, unsigned int length)
{ //Read data from MQTT
  String msg = "";
  int i = 0;
  while (i < length)
    msg += (char)payload[i++];

  if (inword(topic, "/control/D6"))
  {
    float value = msg.toFloat();
    analogWrite(pwm_pin, value);
  }
  else if (inword(topic, "/control/D2"))
    (msg == "true") ? digitalWrite(relay1_pin, LOW) : digitalWrite(relay1_pin, HIGH);
  else if (inword(topic, "/control/D5"))
    (msg == "true") ? digitalWrite(relay2_pin, LOW) : digitalWrite(relay2_pin, HIGH);
}

boolean inword(char str[], char swrd[])
{ //Check word in String.
  char *ptr = strstr(str, swrd);
  if (ptr != NULL)
    return true;
  return false;
}

char *get_char(String message)
{
  char *char_arr;
  char_arr = &message[0];
  return char_arr;
}

//==========================================< Main Function >=======================================================================================================================================//

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("wifi not connected : ");
    Serial.println(wifireconne);
    if (wifireconne >= 60)
      AP_Mode();
    wifireconne++;
    delay(500);
  }
  else
    wifireconne = 0;
  if (!client.connected())
  {
    if (client.connect("ESP8266Client:V1001"))
    {
      client.subscribe(get_char(user_id + "/" + device_name + "/control/D2"));
      client.subscribe(get_char(user_id + "/" + device_name + "/control/D5"));
      client.subscribe(get_char(user_id + "/" + device_name + "/control/D6"));
    }
  }
  client.loop();

  if (start_device)
  {
    delay(150);
    public_message(set_topic(), "device_added");
    delay(150);
    start_device = false;
  }

  unsigned long nowmilis = millis(); // >>>> Mqtt Sended!!!
  if (nowmilis - lastMsg > rand_period)
  {
    lastMsg = nowmilis;
    //    time_t now = time(nullptr);
    //    struct tm* p_tm = localtime(&now);

    public_message(set_topic(), ": msg");

    rand_period = (rand() % 500 + 800);
  }
}

//-----------------------------< AP Mode Function >--------------------------------------------------------------------------------------------------------------------------------------------------------//
void AP_Mode()
{
  Serial.println("WiFi AP-MODE");

  WiFi.mode(WIFI_AP);
  WiFi.softAP("IOT-ESP8266 (" + WiFi.macAddress() + ")");
  while (!data_status)
  {
    digitalWrite(relay1_pin, HIGH);
    digitalWrite(relay2_pin, HIGH);
    popupLoginPage();
    Serial.println("Wait for ssid");
    delay(200);
  }
  clear_EEPROM();
  delay(100);
  write_EEPROM(0, ssid);
  write_EEPROM((sizeof(ssid) + 1), password);
  write_EEPROM((sizeof(ssid) + sizeof(password) + 1), user_id);
  write_EEPROM((sizeof(ssid) + sizeof(password) + sizeof(user_id) + 1), device_name);
  delay(500);
  ESP.restart();
}

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">

<head>
  <meta charset="UTF-8">
  <meta http-equiv="X-UA-Compatible" content="IE=edge">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Mobile page</title>
  <style>
    html,
    body {
      background-color: #76CDA3;
      margin: 0;
      padding: 0;
    }

    .center {
      max-width: 500px;
      margin: auto;
      margin: 0% 0%;
    }

    .head {
      color: white;
      text-align: center;
      line-height: 20%;
      font-size: 40px;

    }

    .p {
      display: block;
      margin: auto;
      width: 20%;

    }

    .fill1 {

      text-align: center;

    }

    .tf {
      border-radius: 10px;
      border: 2px solid black;
      padding: 3%;
      width: 75%;
      /* height: 1%; */

      margin: 2% 0%;
    }

    .box {
      background-color: white;
      padding: 25px;
      margin: 7%;
      border-radius: 20px;
    }

    .button {
      width: 36%;
      display: block;
      text-align: center;
      color: white;
      background-color: #3a7957;
      text-decoration: none;
      display: inline-block;
      font-size: 86%;
      margin-top: 5%;
      box-shadow: 2px 2px #888888;
    }

    *:focus {
      outline: none;
    }
  </style>
</head>

<body>
  <div class="center">
    <br>
    <h1 class="head">Smart farm</h1>
    <img class="p"
      src=" data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAABeIAAAbuBAMAAADEe1/nAAAAHlBMVEVHcEwZGRkqKirHx8f+/v7s7Ozh4eH29vb6+vr///+avdcrAAAACXRSTlMABxY+64ljrtBwTGQkAAAgAElEQVR42uydS1cbxxaFu+8v6G5WnDttWMZ3CsTYGRuCGRrCw0MLGyfDGPPwMPELxpGF+t/efkotgUBCXVWnur49SGyvJCvu3t7aZ59dJc8DAAAAAAAAAHsQTAKPBrQHfoZwGqT/HI8L2C3pJZeX1o+P9/b2zs8vLi8vv2xsbCRJ0k3/lv7s8vz8dG9v//h4baX4ZxF9YDXV3x1cfNlOpkL38nzv+GhloPg8SGAF22en+mTiY3SAbMeesXT93QOpPk780/3VgvY8WiBQ2zNuLqwfXDRA9hrtv+8fleaeRwxkafvC8cGXRAW630+P4pz1PGogge6Ftqth+5D1e0eMs0DGmJo6mUQHUoezgqsHhtV9/eAq0YitfJhF6YER6x6tH2wn2vHy9AilBwbU/emhAboX6Bekh/VAo3c3RvdK6VdJb4Amedfs3SeSPhtkIT1Qqu9huPBeBN3LQXYXzgN1SN3M8uF2Igr9sxXGWKAqnHn6MZGH7mkHzoPGCZ/amXdXiVBgbkDz9v1gOxGM/n7KeV4UaErfHx0m0pEZehqWYH55T/V9/SKxAd3TFYQezO9nRI6rd3CeIRbMo+8W8T3H1zfoPHBC30c4j84DF/QdnQfz5DO28h2dBw/yM58Sq/F1BZkH0+v7o4+J7eiexexhwTR898OFw6QNyPawvE9wr8BH77eTluB6FzsP7jXwV0mLsPUGOw8moxUGfnyEjZF5MEngo/dJ+9Dfxc6D2wW+XYZmiM0VUhtwS0Lze9JWdM+QeTCW0ITtSWhuTW06yDyoC/yjT0nLcRoj88ARgUfmwZjAf0ycQOrmoTwIw2fbiSO4foPMu46gzRENoQ24KfBPtxOnsPWGFazTAn+YuIbuLmbeXYFfvkocxNcVZN5NgQ/fJ26i30HmXQzhFz4mzuKM3rx7IbxrI+vYALtCZuOWwEeHidvIBlisjTsWvv01mvvxjWieEN45Z4OZdyOj+QO2F5nNDpmNCwrvckYzjhPMfPszmkdXEL22jaI133YL705Rcuo6JWa+zRb+EI6Px5SY+TZbeEJJzLxTKTwWHjNPCg9SM08y38aQhhT+jmT+DSrfMsK7dbiPmg0WniLN/QViGN8ihf8JC38vNmNUvjUK/xjCT4EfRDZtUfinsJnIxqWQ5hlcnp7yMMZ+hSeVnCmlROWJ4Z1KKQnm7UYYEsPPXCxD5S1W+AjCz0z5E4J5exWeriRdSqdieAj/YMpDHxsVnmbBQ/ENlbdR4WnDz0V5KGSdwtMsmKdkg8rbpvBUaebulUEji2JJCN9A4wCVt0jhqdI0QnmoZIvCP4GvqLxTHh62NoItvDwe3i384FiUDQrPCT8SG6dyeAhPLo/Cg3koD60kKzyb1qbxGyovWeEhvBLKQy2xCn8FQZsHTUqxCr8A4ZXgMyovM4jnAIgqcCpKIt/9CMIrwwduOJDn4aPXEFMduOFAnMJzTYdSdF+h8rIYH76FlWopz21lshSem8dUg68REWXin8NI9ZSnLi8niOcEiA5cxzBeiMLTHtMDipRCFJ5ugca+AYQTsGqF8Br7BgQ2rFpZvgKtQTybJ82bKFSeIN6tjBKVNxrTcBWT9oySWN5kTPMTDNSOHwQ25hSeQ34m8IJY3pSJ5wgIgQ0xDdAAAhszMQ31MXOBDbbGgImnPmYwsOFCSv0xDVOrSXCJjXZTQ5vGLGjY6DbxryGd6ekVHuqMaTjWahocfNVJ+IBygYjpFSqya3VseoXzmnatTK1CplfYqMfEM7UKsfLsXvUk8exa2b06ZeJ/hmli8AMrz67VLbxg96raxHOQWxY+YOUV71pZPYlbRMFLlSaeqVXgIgpjg4l3bBEFNZWZeFZPWHlMPMDKt9XEc+pJqJUnlcfEk8oDkvgW4y+sfPOEx8RLtvIriHzjhKdOIxkUbBp38QuYeNGgK99wTBNi4oWDrnyjCu/TLpAOuvKNSnyrbs3e2Hh5meHLxkabrBptgwYl3vpgcuPy++ne/vHx2triOJbW1o6P9/bOL77YTv+/sPIEk0n3++n+2urilFhbf3duL/H5Uu/Glq12epqX5/tHU3N9hPfHexdW0p5vD2lI4u27rOPl9/2jxfmwdHxgH+1pUTbBd9s8Tff7/upiQ1g/+GLX750WZRM5jU2NyZenE7U9ilNEYRwW8IPyB3GOyVr/ziapZ/XagMTbs2ydIO5RFGUMz+ld0jzIfhoUv1ZS3/dz9i9Gt0q9Nb6O1ev8Oc1ra71MHJdMDkqWpz/2M3hBqoR+ifzXB4pfyL69/gZfM7fE/88Kuu/eFPYBfzM+px/1Bc+DDCnn/WzEK37ilb9U/DQX/vSPRhyNq/2SFaS/Jq+Zz8Rb4GnG1T2Oh2pdcjwX9pzjRd6a/dj3/bJBUf5SDZULGvf3NtibF+Q1c+U00j3N1gS6Zzz3cmHP/zrT/iH/NyubH45p/fqBdA3A18wj8bI9TXc0manY7ofewMH4s/8pL0k/UPsbpH/3Ubqvwdi0MqfZ2h9V98q0exnXBw7mwb/33PAMZ9p41NJvi/Y1cLd9OU33dHVU3YPKywS+15DEBdl/KJ9sc4MzqvRPP0r2NVj5h/VpxHqa/lmd71GVyeSGvdmXnRG+7m/qjv6CvKZlEi/1to7+/g26+7lt9z0l2hbkxr5kfZ3zy4fkNeQ06u377o1Z1c+l2FP7lnPO59usWL6hJ69pTU7Tr/G9StyzQTVQLWp+mf/kA0PN0i8dCvU1GJtZJV6ip6n5mULe81FVtbqP2vrS0seyvQ15TRtymu7ZmHvXSPX6JBsEYyn9ssDc5g0iP6NpfSKP76sjfC/KMUYmtLyjMMJ5eVklec2M71Tc7unrat2+F20wY3rg55ll9j8yzOelNW7+Ia+ZKYr/U9br2+yMrFbzFpjRFzp0NwOdfy+L81xFOZOJl3WWe+toxM8EIixqPsNmHYSBt1l6L+qDsUdeM31O8x9JctXdrfHdL9phUl5lKfMDzv8uifLcXzN9FP+LRANfxZGeHOUK8nMmdWsjyc73+RbAaU38I4kbp3Jg9YTNY3kpOVtLCdxIEcpP6+I/iRP4fN/UYC2yWZ0f3UkJknlKlNO5eDH1gn5nwPfiTLZAwg+cYM3biJF5QvmpXp2YKP6sFtAEvmS1CsqaWWVtHkuReUL5aTyNkCj+ulMz8L4XeKLlatC3qcJ5QnlrCP+zLIEvLyQQL1W+V65hSzu/LGMY6uFr7Iji6wKfr/SteHhlPC9K5l+R19gQxZ+NGBrPFi9aWZtFQTJPKH+PxEtoxfd3agLvBXa9sRGZlxDacBPl3S7+tYDW2Gpd4C3LGsZk/ql5AenSlL9DnwSMrd2T+sga2BiuBd5Q5gU4G7455A6BioyPrdXImu3tPTuz5Ko7X/7J/UNAo4xQfpLEGx9bN+sC73uWipM/IvPGnU06vELu23Ma4w2yk4HAW8z3kvKBHGfD9TUTchrTY2tVoxlcH2n1B6ZXC20MZzbp8Aq/b5N4w2NrmdHERQZvv0fMQ5vysMgzs86G4fXWN2R4bP02OPcRBEE7ArXczReUXzb7cDkOdVsUb3RsLY/2xcXE2pJHWoQ2kYADgWxeb3HxRkvCpYWPyhpNa56qV8spjcaU/yDyNyTeZEn4enWQSXqt0qKyXFZQ/pnZ2jAiLyeZ3KzXaNoWpKU6Xzkbk8k8Z17HJd5gMvmtltG0LzguLp43P79ywfbIWzGZTJ5UhPeDoJ2bkjynNL2M4mzIyCsxl0x2d2pHnVr6dGuUXzL3WfqKzWtNg4xdX9Dt1I86tfhTNKjMvLGUkosNamOrsWSySCUzC++1+nUE3jCZN5ZS/o3ID1y8qeVTLZUM2v02yjql0QOwdCiNJ5NFkyYOXbgEt57Mmwrm6VBWyaSh5dPmIKTxPRfUxx/UbB6bcZHcXlNKvKHL4r8N106OuMc8mDdJeUS+kPhPRgnfmqbkdCNTqfKGdlGsocxdQvbZNYUvfI0XGKU8a6gsNDMi8SeuhDQ3nU1J+SUjlOeKMi/8rzHC5zG8c1HwwNgsmZAaTkOZOfm0Wyp8/pWs7j3zMqU0QnnXT0P54XMDT32ndcedZo9sTFH+2vFvADRy8mlnEMO7mpWZVHm3Rd7I4dYP7ixa71J5U5R3+sirkcOtJ8PjHw5/vBpUeZePvJroF1SEdzsyGN5MqZ/yLh95NVAhO3Heww+ffpFS6s/l3T3yauBw64mDi9ZJjrIq2WinvLMi7/vaK2Sfy0tp3IwlJ3l57YWDf0MkXmN5LELhy+fvVYnNsu78wNGvDdHeEv7N6cXTbZSvVF53ebjnpMiHuitkmyXhofpNlX9Ma1hDPqa5JfxjkMOj8DdV/gmtYfUuXq/E54e4Yzz8jcSmbFJqPvvqXmtY9y1kFeHx8Lfl8hnl9Vb63GsNa5b4/iqb1rty+YzybzUXynwkXuHOozO8eAzckstH2m8rc+2GMs0Sv4PCT6Pyn/SKvFveUavEf0Dhp1J5rX2DvlsCFOp8tp/Lazpg9sSkuExstPYN/nZp8ao1i/9tcGsBnL/DZuaU17l87btka3yNNeHNkvCw+i6VN7CJ6jmU1vgaj4JcD7+kFWLfo/J6Y3mnmgZ+pEvkqyAeRt+r8kFBeV2xfC92KZFPn60eke/uoPAzfPLmlNfU4e4suqXx0ZK2XBKFn+6d6M0oe4tOabyfismvGh7rC4L4mThfjK9aDoh0Usa7ZBpTMVnWVBBOLQ1kntbX+GGsJ7DpZS/GtVxAucjnfUkUfjYp0hTYdNI345QSZbZGtcjn9TEUfibC65pee8U9t66JvOK4Zqe6twDOz/pmlN/b1HHM1GT5ZBQtqo1rPhPTPETly8BG7fTac4/wgZc911/VT62Y+NnFKKf8E7USHzt336qv2Mn3VzHxD81rirb8L0olPnbvwHGgNK7Jp9YYhX+olY+UTq8OuvhcSyKFmfwH+pJzWPlA7e61cPHuaVGg0MkXlfiAmObBljNUeG1TnsUHTj5WVU7+mmOtc72ZzHLGyhZRPWcHrEBVJt8tGsJcET93YPOnqqDGyYuy/NzJLykz8Tiaud5NsXu9UhTUuDoiqXHyL8ors1H4OQMbNYuojnMFgxEhUeDkr4vVE5ydM7ApfM1zNUGNs49VQSbfLVdPmJpmrPxbJL5pJ7+sxMRD2Plfjh82n8r3HP/8bd7Jv2DXKtrKOxvUqHLy1xQmG7TyxfmQ5wQ1TTv5PzHxoq38a1x8w05+CRMv9iM4o3xzVr5Hitask+9h4pVY+cdIvEwnn3XiMfHNWvniPo+3SHzDTr4Zkd/BxP+fvXPpalvJorD1D6rMCt3Dlt0BpphA6DFJiIdxcAjDQAJhGPIADxtCAuPYsfVvr1QlyfIDbEmleu6zQga5a92F7K2t7+w6VaoK5XsIasSS/JqoYBIQXwHKi+IaBDViSX6IcZoKUX4XFK8dyYefp/8fWEgVKC+Ga0DxYkmeH9YBiK8K5csvvcLihZL8gAseH2clD+HwC2pslP+GYPHiSD46uwC7nqpDeQFLrwhqhJL8DbY9VY7yzTaCGqEkf14yp6FOngghx5NqLK/ZAMXrQvKcaSD4Cr+hslyDoGYOyZ+XZRoIvsqncDmugcXPxgHFTX4QL7aC4qvDmpLrULD4WQ/xC5t8zDRoWyuWfBmuQVAjMq65wcSknOa1ONcgqBEZ1wzYPA0WW6v+gsrkNR1QvECT50yDj7NyyZPC8zWgeJEmv4e2VZorsfkaBDVqTX4EwUsjeX60wTNYvKhPtJDJX7CheAhensnn3+fdQVDzUBiQ3+T/Nhx934Qiko8kv54/WkBQ83AYkM/k+y20rVKLcc0RghqBvWs+k782K4r30qKsSFrmNFu5Q3nMxS9wkDwmP2xEhKi/YDIiZ1Vvbm3tbG1ttTL/ZsZbqzjX7MLiFcU1xwbse2JHvkQq3zrtHl7d3b14MeGQ/Rev7+4uv7872YqFr/WulmS+pgeLVxLX8CheX4XExr5z2r28e7H4cl7/vuyecuF7Oku+nuswD1i8uLgmaluptgNkzNpXTg+vck6i9H93T3yq7Y0cn19zDotXENdc8yheV5JZ2Tn8WXS4dr97oq3VM8kv3bxiaFJcXDPUNIpnKLNzeFX2YPDf7zZ1tPp8OwCRxQuMa+LTCzR09xLmPmX171r6YVs8RNkDxUuOa/Y0fC03l7uw09ZZOxs5vV65JR+ifAqKlxvX8NVWnba2RjSzcvYzEF4Mb7TySbrstuRoogY7F8TENdc8iica3an007egmurfnuhk9GTZlVdG8WAaIXENa1u10UBE7ytiaWYW6fVhuDiU31iK4uHwYuKaDj+RSZeHPN25Cqqu/ndt4CZ+1WtvGYqHxQuJa17xZJJo8e1XiDNTdXusi4KWGhuGxQuLa/hqqxZRfNSt3gfSav+tFhE94SZ/tNjiIXghcc2NJlPx4fe+ctYOpBbTvCZf0tri5VbsThMR14w0mYqn8vWui+bjlddnC5dbgfEi4poLHQ6oiTZHyOSZac0THb6kxxJKLLcKi2sG7IVPij/L8Km+rUjvTPMfVKuJr7xuYLlVQlxzrH5dI7wnt78FSut2U7XNs7ymh+XWyuOav/wNZ2q/61XFeo8Cqx9q73viRafjrj9G8dC7iLiGJ5MqE7rQ4M8CHWrUUampeOX16BGKR1AjIq65UW3xlK71Ak0qRBt1t/5jp/INcIyQqLhmpHhva9ivnQX61OitUpt/8CWNCGqExTXXave2Uvr0PtCqog5W0cdByEMJJYIaYXFNbPHK7sOV94FupdDmOdc8e+CkSQQ1OeOaua//u1A5q6E2gteR5h8weexuLQjMsyY/VDgzqaXBx/mVKpvnZ3lszB+ahMXnNfk5JN9RFwHoR/Ba2DwNH8WNewxNCntins8+LZV8teEv8znQuRRl86Q2Z9YAWbwwkz9WFQF4dLUXaF6/lHgBH6/pzVo8okkBJh/PFxD5Nx993g60r+EHqsQMpndD4ZUgwuKalqJkktL3gQnV78hPBL2ZgTIENcLimj9q5gsIXekFhtQP+TDPl8efIqipgORbStrWEOHvA2PqVj5PhCTvZwbKcICBMJL/E+188qTfdfR5YFINN6l8T8iafAfRpCiTb6kIanQPJefHlFIF503QJyZqhJn8HwXnTIYI/y0wrfpfZVts9lwDbH0SFte05J8zaRbCZ5N5uZLjmfz/0qAGFC8irtmTHtSESLXdDoysfdkwzwbKxkENdFue5Pst6dsmTetZJ2D+g1SjJcnUMIIaYSS/x4IaT+a9ZlzPOiV5qc2rx6eGOcVDteVNnlu8zI+S0o+BydV/Q+X6Q2TyoHhhJn8iOYsPO+aDwOzqd6SyfCT5JihemMnLblsNGix4uL5K/Mh4XIPFJ1Fprx+9uFWexRMrBM8kL5Vr6r6PuXgRXVH0Ll+pFm+J4KW6PHutJ8UZNUI+S/ZWMXkWT6wRvFyX55KH4AXZh8SFa4sEL9PlCXtXOdQqqnv1ZD0tQ4e/DyyqG1ku70WaryGpEfNZRmQjy+FXrRK8xCGbSO5gGnFPTFkOb5vgmeQlehPKrLJQ8PJHKVHmPEqsFLxUl0fB4eHyKG0dfsVSwUtMbFAmObxNObzKGRuUGRGoV7dY8EFwgcUh1KTD148Cq+sNBgBQWYenB3YLPuh/gMujxoo3fMfTkpKHy6OSmGY3sL+Gm2heUbHDrwcu1AC7rlHc4Z+0nVB88IpC8SiLl1rnrkRB84hpVpwRfLQShW8cEH/kjuAlH2OD0tHhHcglJyS/CZd3HOKfB27VEIGN4zFN4Fq9RGDjssOvtJ1TfHADlHcX4u2el8QcJWo6pjlwUfCYsHEX4ncDN2vkA+WdhPgngav1EmuvLkK8S2uts90rFOAexPcChwtrr+4J/qPLgg9Gm8AaxwS/HrhdA6C8WxC/2nZc8UB5QDxQHmWv4J9B8EB5lwT/L+gdKO9QERfnx4Dy7hZ1atcTUB4O7+1C6hiwccnin0DpaeE8DweS+Po9hD6uL0B563Oaj5B5pvqbyGvshngEk7MRJWRhs8UjmJyua+Q1NkM8gslZrsHB8jbnNOtQOJZeXbJ4MA2WXt3KacA04BqncpoNqBt5DXIaFI4pQ07jHtfA5C3MacA0yGuQ06DSdShoxLac5hyyxnyNSzkN5mker7/IayzLae4h6sfrC/IaqywehxcsKhxtYFVOg31Pi2sPJm9RFN+DoBfXG+Q1iOKdqiFCeUTxCOVRiOKtbl5h8jZYPNrW5ZtX6MUGi0fbunRhUt6GKB6rrbkmyiAZ0y0eq61YeXXL4rHamqt59WHyhkfxq1BxrsJ2KCSTblV/EyZvtOCRTOYtHDdsdBSPZBIJpVsUj4GaQgklXN5Ui8dATbGEEtox1eKRTBZMKGHySCbdSighHiSTjiWUMHkTLR7JZNHCDKWZFo9ksnDB5M0rzEyWKZxeY6LF30O4WIZyyOIJFp9KLkNBRGZZPBafsAzlFsVj8alcDWHysHjXTB6D8ia1rbD40rMGMHlYvFv1f5i8QRT/Xwi2vMn7MHljLL6urcW/uLu8/N7tnp6enJ5+6nYvL69+avu7XsPkYfHF6/Vl993JVmNeNbdOu4dX+gm/D5M3pXSz+NffT1uNhbX1STfVY6DMFKj5t040vJTak9rRSvV9H1hjhOC9VX3M/aSRt5rb2oh+AI83o6geW0H23z1k7vW0/Aes/lCLMbgviOQNUXx9TT3M/JiRu+/7dUopITRbXvgT/pdZ0V+pt3gfijcEa2jjXLW9T7s6lzZhfzzqxcX/lf9FQ8ufxJtDxXTTaUDxhnB8vaHU5H+fTFp74uaR2D2vRkj4h1f4u0Zhamj7ifFPmH3zTCXcDBvweGM8vq7Q5G+PJ72dMpKJKlJ3qPZajaT3ZvTj1dhtwCyfe33mf/Cpp9DifXSuxlCNMpPP6j12d48wOYdCZ39N/a78J1Q9uxtiwMk6/bYinx80GhSLrsaENQ01Jr9/PC33SMZeouvH7lLm/R7jHW71Y82fKeH5i/ASADWmgLwakx+9TUXKzT2GFrK0U0Y3B0OcSbppvldi8cB4g7BGgcn3x/kMs3fO5kV+fZJofkw3a+/lU3x420JK5iTy0uOa21aKMyTuVWegPY/VRxlOlm0k4zws3jjJyzX54XHW33kuU6brY3QTpTdR7qQC5zvoW42qEApkknz/x5jf2fJSYXefvowJn1/7Jtni4fEg+bn1sjXp76QmzBy9GlujrctGG1C8aYKXSPJpQlNP8hmR1+F5EzwvKbWBxZuHNdGogRSTTw2eLTXVBPp7po3NaH67LcviIXizJE/lkPyPTADP85kK7l5u83FUudaTZPEQkWlxjQSSH3XGQOPVKos2ouQnY/PvpVg8ghrDSD7EmspJfr81HiioUPAM0ticWdzBPm+D4lFzuKZqkv81TmjyjBIUV32q+bX7yi0eAjLQ5Ksl+f7blOAl6J3NVo4l3zyo2uIBNYY2r5WZ/Og4JXi+xCojc2WpDb/RPoPiUTJJfthKd/PVPImXNJb8c1A8Sh7JD1oJ0RDJz/9Q88SvMpkHxYPk5yw7JS0r4Vs5JN7FbNaGw/zTNiweJYXkf40zGsmCT+5kWllkA4sHyc/U15RolFxVRvLNHoIaVOUkf5EJJRU9/VOyaR4hqEFVTPKdhGhqyqyQsCvjMH8EikdVSvIdVSHNJNmQNKY8AsWjKiT5xOEV+6DHhw5ESx4WD5J/CGkUZDQzmhfv8rB4kPzkKE0ieF0uLZb8gTiLp7B4kPys4Cva+lFA9GIl34kGnyEZkPw00niapHf8qANxYIMsHiQ/u/Ckvmmd6/JClqKQxYPks3WTOLxOF5e2r817QRQPxYDkeb1KHd7T7I7mkl9riwlqIHmY/HhakuoX3Y1dvuwkJbd4QI0Vmi9v8sNk+4fnaXp9jca6gKAGgrdC8LXSZ9ewHU+aNa2ZZ1gs+V2cUYPiIF/27JpRK2la9fTARPKfQfGoce9a3OT7x3o2rRMszyYpP5aj+BqgxpoqZfIXyTy8toKIwI25fA9ZPKpWduF1jwueaH6F7FzKohklsnjruCba/39eVAyaO3xCbhHYPC2m+GM2UQPJ22TyRUmeda36+x8pFdhgosbS5vW8TNeqvQF63OUPClk8KB4kP+5afSN2SUTHUrJXwPZA8ajCJr9njMMzsinYvYLiYfKZrtU3xf7Ye4+LdK+M4pHFI66Ju9a69jFNpjjKPwPFo+K4ppl705NJgJtO2PTyL7eiQPIM4uvabGpdOrDJi/IRxRPs57bV5HOQ/JC/4smoxz2JUX49J8WDaezUfC6TZ0m8YQ4/RvmPuSgeDm+n4NmLvJcm+euGiae3eB6X/NL7XgdoWy0G+VqOuGag10kduW5smiOi5BQPcVjcu64tHUz6RkYYJD52eBcUj8oxJ99hXauZEQbhkj8CxYPkl1545dMFZioh2RK1TEQJineCaxab/MhUiM9eZmMDFA+TZybfXIppDF6HTFB+Idf8BcXD5JPzx0yF+PQy60s06aB4kPw4pyFmXybjmt2FFg+Kd8DkFx41fGE2xMeiZ5LvgeJh8gtHKP82LDisiJDF61Cg+H/YO5vutnUjDAvxHwDIc9MuK7H56NJiwtNsG6aWlo0rM11aupSbZa8dm9ne0La8vowj/NsCIO34W/6QwAFmJic/wNDLB+8MBgNy8qafpvY07t9zNpJ/Qy6eIL+ghfI3cFPin2TlT8jFE+S1Et7d1U8jhA/94vXR6wty8QT5u8o1Vd+XGbusOYf6lTpqCPKafe9uf/vG+TrN+bdt+oaH5OIJ8reWa+Y9j5405fU51Gty8QT5W518nbb6M8ni9qI8IR6dk49uTVu9ETxjtyavpzSGjJz82eQWn/b6Onn9fMsYMjI12J38e7dbJm/+Q288fTDHrSQE7JCf9829J7/IZ3zNGxpDRpC/AfK/1/aldgwAACAASURBVKV4r9xtM7/m5AYXTzLADvkfPlUmL3zc+i99RYgnyF+D/EcvyxdNE+VXQjxB/kqf/Gkzgsy7j9sU5V9QoYYgfwXyY18r1OYvvfhxU6GGnLyKP+rKJPfyL738cZOLJ8jXPZOePlrdnLy+IRdPkL8A+fd+3AO5C/JDQjxB/hzyCvFdf8F3CfKEeIJ8jfjAk674RZCnQg1Bvkb8X3wGn4H8X6lQQ5BvIP8Pz19pZ7XkT8jFE+QN5OceNtTc5ORfk4snyBvI1y1k3O/Pu4Y8IZ4g34vmvowvuOvbNh/3K0I8QV5J3gzsYMz3r1tLvk+IJ8j3egg8zXkPZVf4vptRLIR8t9vFwD19AbDbCwjxBHmhVIBgjoWGvA760dFrXggc3Kv/Uk5DapALXvkajoN7vCMEDWWi4EyrnuOQvP5rSfIk+Q6WXI5z0juFcTZ4Pm/6te2ghVO+ROF3VaCJuh4WT+pYp5Wh8MsQiyAIwijux01MJkk8SZJ8ZP5tkeIp/HEtNcyVwKdb062t0YGOWRODstT/j7fptI/C8YxPRGGoYT5NpirykYrsXOvl5edYqsNtWjIKh7Guoa7knkxrlZdpOkh1VDqMxC/POkw3u5S9UriYlQbrsRZ6zfSDrFBIHwwGQ7kgjseCFE/hItjjJFFYL666lgVRHXXpiJvCHbCzoAZ7XnsYlY4O0ocIXm5sdonxFC6A3VRhulE8nT4c7BcR3yfFUziA9rVYS91UG4vZTKWnjxO8HGzSBUsK0GIXoheFkdJ7PlJafyTZL5Qm+6R4CsAZKtcuJleWXYN99miy/3xUbMv/cQEUbrI9MEdKk1rtT1b6GeI/THoBKZ4CHtsFi5Np/sUcmirTPlyW4vdpTgoFOLjX5cfRMmz79dIkmRoKWDUZEaocNdO9AirkkhVfHRLiKQDBXVdkTJr66Gr74tJklw5cKYDAXcTJKCvKtKrkikKXJsnUULQfoj5dMkWZoVxdUGmSAkJdRoi1OB8V5Sq13jz/PunROywUrcpd0123DhSzMl254Kt9Om+laM2468n5YaT0ns1KaScGEzI1FO159yiZ1s0D6dCO4KsvBvEkeQrbVoaJnr6SOjooUmkx5lSapGjFz7AgNh3uS+gLe1Acj3sBufjLVYOfQauxGvMuAkP3rCil7UhN3kq/bIOdZmjVxWCM6lhLL82EyVTfX0ptefeLeeuYXsxtyK7lrTfag3qfTQez2cFoGgssA+xt4T3S3ZDZwSyVbYTJW2nzNpcn4zy7aY+tjr9M+4b1pNcl7KIK74opadqO3s9aasjLiCS7c38d7K8LIsMS8G7u7JVD2VocjtE/p6jwHu6c3KOleqtL7uZpeI+SUQup6uUde7fX7WJGl8F7cW8LuE6H04+jCmMA8N6UJlGbGmXP870HLdiEOP+oZY6SvG28m9ClSbyCV37m7clDl2xjTJx/YBEs1AWwld3ueNivh7lrUvmZt3uPyn22KYe9P96DKE7yDILakU+pUQb++d5jF+6IjjDuiXdh2mZstADfrzS5i7alRgF+5wnYmX8kzC9eYz3MPc9mMPB+lrfizMOeBPjG2qwT5u9eYram5K7xDkfxKdarIBrwT+85JczfZd91K4GVG3w0peY+JZpwbxnrt09Fm9tXOBkVaZrCEnx1hBPxQrxd0g+xsU21+ZsWWM//zUBU369c6EaJeGXhPy0PGpsY98hF6xtOsyKtJLz40MeYt4rg38tcxF0y81f43mIj8KLMa1+31OAzmF+Xu4xHRPmLfF+bQjlsusHUxAjPW8Xzk2Wv4yFR/qwCVtv3FKjgMU6p4SsQvHKHXVI8M90E+UEp4QbGKTXil5VsuN+7RPm6mwAs33G21PAVCV4ZRNSUrwcT5HD9e5O36umqggi/JMmvY6a8KUfO0hS04KuNPrIpNVy8WCGDlORxqp2zINaDOGDz3dxfw/YqiHi+0h/lO9Ye1EDfbUrBC17OJ7im1HARnqx2Rf+Jry7POoHuBi5SCT9M3ooJSmzZB0/X4xvC8+tI5aupC4KX6SaqvJUx8Z/VL+pvAhXfzRRs0AX4K1dBMJkaJn61sXH+CxPleZBkrsjdnLd2EZkaLv5uJznCUrBh9YETpOt8i89bMb1JL36x1aqEBCNCGfjCHbmr+ILpvJWxVZdpcBVszP3Vg1npkuBxTVcVwVd7S7vrvZVnQZRkhXQrMI0wYHay1gvZq+fLGZmOArcEn+5iMjXild2jPZ87bJjuGDsopXQP8WhKk3zFzQXX44O3fpExFucwpkc+cOM9QvTwE7Np4v0+e2V6XmpxIt0LRHmrZRPvtZXnQiWsaeqg4CWmlhrLJt5fK890S0HhpN7lXJ+3CiSIt23iz6ry/u2VCfQrTrfHBp77rVb6x26K//ll5Vmsj5xcFbzJW3EInvPXbe2jXe4b4KWzMcDTJ8zC1rD03h/IKwc/chfw+irIGMvpExOf21tnX+o1QvcUnEiHY76P5X4rE39qcZ396KLkIoI3A/6hv8QEi+DZWqto+ubBKusXtl128LWpQdNSI960u9LOF+UZC/Nslg7dVvwPNKVJEba81I63yrNO6D7gFXg+9NG4+M9tL/a22wstEvcB34zew9BhwK1d9Ls9vjsMebbmA+D16D0spoaJr+0vt7Mnr4yxZGfPfcDrvBVJaZKLP0PYUR2FC1+L86zwQO+I+oQt3uW+u0LJnNwfo7yUXkR13MeC+DcgFtzJ+TU8nmaOtgXfdN6K4+GnFhtqXG+vYSLM/TA0OjaQzGWCgnhzDOXY0um3D0pfBF99QdInDAbxUv7hFOSVg08ybwCvTA2SvJWLd3AWfdudBWedIFGA90jxWPqE2XNAi37qzoo/i5ORT3qX6RaSecLsb5CWfd0RW6P7xoqZT4KXG2McpoaBYrwrrQYsTPLixCe913kriiYy0X4T2QUf74TiGXuWZGXqleDrd3A4CsUHL+Eg3o3qmPCjb+xyHKO53ypEDwzkxy5QRlv42XDomeCr3R6WSWQiAAP57y7sq0zPg0+ll4jHMTKeiR4UyI8deBedc69Onc7jqI9mgjYXQS8Cgnj4iy70QHgPFY/pHRwmoEAefj2YKcHv+Uh4RHlrnbr2XkJBPGQnqVJWLy28RPZ+K+8AgfwY+o0z4fq8sQWmBgvilZFXio8gIB604LluhS+9tDTNOzgdPAEC8srFdwHvqyyIPbr7cSXmu5jeb9VX8QE4eeAunpt5NJ4KXn5A9H4rmHINcBcfJf71FVzMW3GZGqbPXVt28rWLZ1B3QZEU6dBXTyM3UOWtdbmGd1uGPGQXzzpOvkN8f8QfIkP8WbnmZbuIh+viWZB7rPdmaAfvIIuWnfwYcOOeSEZF6rHg62GTyPTOGGvVyRsXz4Buf9HOic+El9URmqfOLv2wrTp5uC6erSWZX9dZr8WPzR6W5yzB1ORPwbp4FsSZ1x4e0yMJNyWvn9tCPEwXz9ian73BlLe26+RPobp4FiefPNd7M2yywzFCvjUnD9TFm5E0pe+CN8V4jlHw7Tl5qC5eeNw69jMwXX6C4uRhunhF+J0y9V7w1QbSvLVFJw/UxeMgvKz2kTzuB8jJw3TxXBG+8l/wco7wvLVlJw/TxYfJCAPhpTzEa2racvIQXTznka8XWq8G4ry1JScP0sU/876z4CyOJxrxHDHkrTt5gIhH0FlwHmZoB169t+DkDeKBCV5PysZBeNTF+JacvC7UwDI1LEh2sBAe1yQyEE7+FNykScbwEF5P0MaO+MbJv7Pp4kEhHhXh5eAjkje6Fzr5yB7iYW2qqAhvJmgjNzW1kw+sQR4a4vWFJzyEp7zVOuTBIV7EmAivn7MkxFuFPLBCjfLwqARfYS/GW4c8sEIN60SfEOmdTI19yANz8QIX4ZvxqiR3e5CH5eL9H0tzzdR8I8TbhTwoxPMONsLLDcyd8W1AHhbiWZSXElcc9rtkamxCHlKhhrEQS3vwecx3MU4iaxHyoAo1LM73kBFemxpCvE3IA3LxZi5NikzwFXUY2IU8KBcf5cUQG+J/bFLeahXygBDPxds9bISX1XdCvFXIQ0J8mGToCN+MqSGhW4M8nEINFwk+S6NMzYQ6421CHlChRqDqDz5HPNrxqi1BHo6L5+F/ERJeVltkamxCHo6LF8moxKh4KsbbhTwYxCPsLWhMTY8UbxHyUBBvTp4wEl7ON0nwNiEPpVDDwqSQKBG/0ScbbxHyUAo1PHiLrrfgrBhP1/1sQh6Kiw8wnjyZGGyCG33oM+SBuHjGXmY4CV/PjCfBW4M8EMSzBGeZRuet+3QXxCLkYSBel2lKpJ5GDuhGt03IAynUYK1L6vhCpsYi5GEUaphI9rDqXVY0psYm5GG4eI6yfeysw2AM8jVRTyEPwsUzFu2gtTTmIZwuId4W5CEgnnWinRKv4mn2nk3IwyjUBIhNfP0QDgneFuRBFGqC/7N3NU2JZEu0rv6B+ohxTxHjfvCBfwAb3IpPXUsYum6V6F5P07T7O2Hxbx+3ChSwynl2mJk3L+f+gOmKiePh5MmTmd1vO6xp5jDjGUneB6MmTo8udxnww7M8BeC5SN4LFZ/tsE3jEgZoP/GRvA8qPsludlnEY4cBK8l7QPHx3m6L+MqpAaR5SN4Hit/r7WxgsqJ47DBgJHl5o8atEN5phsfJYk6S98CoMTsu4ufzAQZc+UheXsXHSe/HbgPefkFsko3kPVDxpnfV323EFxA1fCQvTvELEb/L+bFK1GAxExvJe0Dx7ZuHHUe8nWKHARvJixs1Ju3tOuDnxQQLhblIXtyoMRDxC1FzBFHDRfLyKn7X7hPXiZqfqFu5SF5cxcfpLg+BrETNCKKGi+TFKT7tXe084C1EDRvJS1O8MW2IeMQmGUle2qhx65h2HvAYcOUjeWmjxp2vBODnAyQMuEheWsWb3o4nJiFqeEleWMUjXbB0apAw4CJ5YYo3+zffgPi5nUHUMJG8uFFzeAW8Q9QwkrysUWN2fXfBi1MzxrZJHpKXNmp2ewHZ68NdEC6SF1bxcQ8ivhI1CArzkLywik+O4NNUogaLmZhIXpbijUHvCaKGleSlKR4BMogaXpKXNWriBM1WiBpWkpc1akzafQDil6IGd0FYSF5UxZuofQ6sV+8esyAsJC+s4jHo9yJqziBqWEhelOLjpPcdWH91akDx9CQvS/HICL+8/j2cGhaSFzVqMPcEUcNN8qJGjTFdxAvg1PCSvKxR00a84LX9BKeGg+RFVXycdqFpVg/7VT+D5JN/JXnZRE0PYyBwaj6V5NP0X0heluKzW+yneRE1WBrPouQljRp30Q9IfxE1uITDoeRljZoMkcnXdwIznkPJS6p4sw8rfsOpSSHjyZW8qIo3h9A0a+0niBoOJS9q1GS3oPh1UYPpJ3olL0nxJsH2gnVR8xPtJwYlL5qowebs9feM9hODkl9QfCoWEk5732HUQNTwKnlJFR8fwZmEqGFW8qJGTXbzBMS/vgJODYOSF6X43jkAv95whaihV/KiFK94tpWi3rZTZGrolbyj+DgW+d9sIr07yOwvKlEDGU+r5EuKF6IVk11qpfjhjKJt5qLxMTieVsmLqni1cyB2dvMEUaNSyQuqeGPaWtcX9B//SzGWW1zAqSFX8pIUv6dWxf8a9Sh+nYpx3gLF0yp5UYo/VNp8srMLkuU6dopoPLmSF6R4s9/VGaixw0mHZH8alhjQK3nR0OSB0g01w8kRTTQC0Xh6JQ+K/w1cPl7nNOm3GUQNtZIXpnidmmY6ztskFF8eBgFESZW8JMUr3TNpZ6M8p5lh6Y/keoE7ouQlKT7p6aT4wQLw7TuSAsSdwgHgSZW8KMWrPM1thxedPCdqIzxC1BAreVmK16lpph0yip8jRUat5GUpXqMzWTyOczKKH15jhzatkpeleI35AjtwgG/f0fzX0XAlVvL/AcV/PFzQoaN4O+lA1BAq+QXkJRM1Kim+rFrJKL5/hmg8sZLPW6D4D4YLFv/TyE47zOBN0pJ80mpJUXysUsX3Z07Ep2TXe1zDFaKGkuTdk6L4O30Ub2fjhaZJySi+jxQZOcnDqPlw1dpqtS8tUS8XooaY5BeYj4QWGGik+OHEMXySkA3mTtFwpUWdQ3sMiv//E8Jjx/AJGcXbCRquDDQv8++mCil+VgGejuKHo7wFURNoCaGP4u3gotI0dLsXBtctyPhAX3KsjuIXkqPsXiS9J6p/AiuFw6V4fQsMyvyYAzxd46y4hzcZ6kuP++o0zbhMZCxEDVnC+XkEbzJYite2wKAS8Qu8x9kt2afjhmuwiNe3wKB04p2mMYdkJXd5GQToCBLx6nbU2KWIj+OU7ginW6KN3GSQgN/TRvHl6gK3YiCOew9kf6yDMURNoM5kdqVN01QifgH4hPCayQlyk4E+dcuEh4+d5RwBJcUvZDy8yUApXtskyOx6BXhKii/gTYYq47UFDGZLYzKKzeE53R+rk/Hg+AAfpaFN8vqTlYiPki4dxc9P4E0Ginhli/f6K2Ny8eltwmxEATc+UE2T3uiieLdicgn4hPJEG2R8qK+tKkNWrphslWWrof10uPGBahrCrC0J4qelMemgGBvSK5yQ8aB4LwC/ShdEhna/TjnwB44P0IwnORZGGCDLl8fMY+LJXMj4QOvWRJU1WUxWTrwLfJJ++jMG/gK1Jh8UId7OKk1T/bHSuqoD7O0Is24l7NJTBMgqTWPcEMv+LWXg004RqglS1LTPFVF8GSBblZPJISnFFxeQ8SEC3nQVUfzSmKyGNOK9Lm3FADc+TIrXZE3OLl5F/OLTL2k7u5DxYdatiqxJW022VmN4Zq97Svx7goG/AJ+m1GTZe6qq1qgc26JF/AQyPkQVf6goGD8clTsmzfLTibMRFv2nEF96rOd66yoUv/p1Ih7bGqL/FCLFKxrottWg33LXOP1eWBSuIb5E0UB3mRF+XeWe3dL+OrnCFYgPjuP39dStxXJ5wYvHRPy36q64QtQER/EHaih+5dO8fDp1NsKOIONDrFu1LCJbzT2tNA1946y4ziFqgqtbD670UPyGTxMnN9Qe0+wa55+Ce3tdLdbkMiP8ehOOPhuBwjVAFZ/dqilbJ/m6T2MYxrZQuAaoag61bKlZLc5++XSGpYHouAb34rR7qgTxg03Ac1D8/Az9p+AoXk2/tdiMF0RRSj+2hYxBgE6Nli01dnZUAX6FwKR9Tv6POqsGIAkL8ftaFmi7nXvpulXIQPHOqgHFh1a3Kum39isrfu3Hqc0gxyYYBwmubr1RImqWVvwL/ozhaCPcQ8aHRvFa5lu3IpOu4maIv9mvsGpCq1uV5IS34gWubv2D4cvtGez4wJ6W+dZqe8GrT8P05cMzWDWBUXxbCcVXVvyapjEHHJ3i2TUoPqyX6KhbywRZsuGapCzxN4d4QD4kik911K22suLXv3yfpVM8BccHRvE6rlkuy9bIcFP8/CcaUIFxvI59wuWCmg15YfZ5Es6Tv4D4oChexz7h4aSTb10QTpg6xfew48Pi+O53DZrm5RL9y4u5li8A8WFRvA4zvii7rRvigm2IBS3XsF7vXAPFV/s61qerzR7XsnsgPihNkxxrqFufR9tWfBRnXKYqEB/UyzSY8f03gRpOU/UrQgYhWZMqkvGD8VvAp2xDLF9hTgb0VGzQHk7ybWdyUX98OwXH432Y4jVMdC9nW6ONupXxDic4PigzXoGoqeZANspWEzEOsQDxAQE+Pfa/bnWBmtb23seE8Q4nEB+QqFGwXtUOLrYyk6U1ydc3s9Dx4bxEwXpV+9h5W7YaxrynPQPHB8PxGs6CrDYJb1A8Y92Kob+QKF6BqOk/drZmW92fapsxGjEExwfz9rr+nwV5G6hZUHzMWLeC4wOieAU7450zmWxzrMnuTsHxeB9W8dGB/zvjp9d5qxVvDVbHrHOKQHwwL/XfqRmOHMVvi4qMdU4RiA/HqfE+Nmm3FqsuuwiHrJF+ID4YGe9/bHJQUny0adRw/zZhQVMgL97z/hCO/VmTL4ji7G4OxON9XNRk3l9JqGLxb36bmNNvQHwoiD/8oYLi3yzA406/AfGBiJqUs4vzexGymuYT13bVTcQDLiE87wdci0lNhEygUXxyjQv1IWga4/u2SXsyrgN8wl5+TDvg+CC8Sd9FTTXcarbplf8vFYgPA/C+r9C2g6M6io+zW+7028+/oGpCKFyPHjRQ/Bujhm313gbigReIGnoV/2bP5NJh6s/B8XgfL1xTz6efmih+nz/gDI4Pwqlpf/e83VpL8VFywN82m2DtZAiQ91zU/LqoaT65LJDATxMudocA+OTOa1FTrah5e2DPZAL1Ng4mhID4tt9L4wejvF7USMywAPEhyHi/L+HYxwYVLxL3BOIDeJ47Nb/cfYS45mpw74H/uy0QH4JTc66R4k0qcVzc3qNwVf9ivzcKlxT/1qhh3Se81hkY4Ua9eo5Pvb5Z3ETxUdKTqD6KETheO+AXXOl1u7We4t2yydO5DMcDNLpf4nU03lF8UgcyI1G3AvEhqPjE6zMJjRQf34hoscEYaye1qxqv5/2aKb4t89kDDHar53ivNzMtKL7VqovnxiJ1a4l4cLxyN97ndZONRk0kZTCB49VTfHrnOcXXtltjqa4ZEK+e433eoV1Mmyhe7GTVtAPEK/cm//RY1DyP8vq6dfHLdArE4/2WqPHYm3S5eAf4txAzYuX2CRCvXNTse9xwbfLi3US31C/TT6yrUS5qDvwVNW6BQa2mkewhfMEqA90U7/PW+IYFBhH/Bu11jkc8XjfiPb7v17Cjpvpqsb/Tv4F41YD32Zt0FF+r4qNY8NTy3y1Urrq9SX+PFg8aKT75U4ziLTheuVPjr6gZXjRRvORXY8xVu4y/0kfxUXIgl30rgHjl3qS3ucl+eY++1ggUWK+6hnhEJ1Uj3l9vcnDdRPFxdimotbDKQLk36SvibXn1qVbFm55g02w4AsdDxpPFhOuxlUkmgQYYc1Utano/PKb4esAbk0m2EAZjIF7vi9ObU30Un4jObAHxut14b2e6J43WpEkvJT/6BGOumhHffvCZ4uMGJSb60YjH65bxT54iftp5R9Q8AfF4gcn44UWnCfBxJipqgHjdiPdVxjfOc8uvDPySQ8ZDxn9+TLik+Lj+d0lWiX1pYQQKMp4gYJA2LGkXXxmIESjFFJ94uja+aOw+yYsaC8QrlvGJpzJ+MGqYbi13xsv+lVqMQEHGfzqops0Ub9rnsn+lGAhRLeO/e9x9qlfx4rfFMRACGU9B8WmDHyJ/WxyIhxv/+XVr/p6okf46hIUVy3g/Rc3JOyo+OX4Sl1y4bQkZ//kU3wCqOJMWNfNnhIUVy3gvRc1g3BgwcNf95h4gHpDXKePl+bK2bn1s3GDgFn8/yf9BguPVyvhzL0XNexSfyd/vQXRSr4zv+ijjq+5TQ1Qr9uDS8j9AvFZR4+fZ4vKWZYNQNj5cMznJgXilL/VSxs/GDScSysrDgwbCCRpQWkWNlzLeTjoLim/65D89yAGdIB6vVdX0fIyRDcd5I6LMvg86DGFhrU6NeCTro6lJJ2o8+EIgXivFpz72n4rm1KQnd2ftpIXKVSfHe7lwcvYOxftxsKqYgOOVcvyhhzLe3jdHaoS3Tb7+CgHxOiledndjE8VfN1uTUerFFxfYpa2U4r3sP5W7Jpv+Rv04WIV4vFoZf+cf4N/ZNbn4Yj+ucCI6qdWb9LFwfTdS48n5nmfEanS+5I8HZRQvvG0SiNcu4z28W1xeOmvqtxpPbhL+00HlqlLV/I+9K1hOHMmCqiZiz1VSrM8DCrvPhgDvB8iLfG0a8BzXGuw+r23M3sGi7zVh6W9XJXD3dMS4aqYRqnzl0qEP3X3AkE7y5cv3XohXuErNIjIUp6ZCvI9O0kR85wsxUdPBEDXl796Op1m4nsAVrtpIDWMgDTPpo5NEC9cTuP5TsdRQfCAsr9D2QTLiHC9O4WT8OHtzEVkFeJRlUgrxnuMJPuEFKVGDE+aXPkhGU9RAxBB/9LknWlGDEub30UmqheuKFMWzCGVDZjHziKdZuKL1n3Z1K3/rNxRmQ6YPkpF8+IfhFRjF61bv4Tg1an+UD5JRVDURWuEqn9+8V4zk1Pg9q1QR35mj1a3KmuTwTk0dJPM6nmDhGoEVrnLT09StQKLGRyepFq5gHdd08vZ8K5JT44NkRDlegK2qkbnWmmRAu6T81kmSTwhm1ciljuID+5dwfgiSefzQEzUR2HKmcfb2tJ96uTgrYeXGx2pImpNYN/7kpt/VUCfSgkwfnaQp4yOsGVftKIhKveFUHT5WQ9SqwSpcx/2eRitwpD0jPlZDE/FYhataGa+heIa0LtDv4COpasC2Cqc3OopnUMvTisy3XClaNViFqzYnDFZm+1gNTasGqnCV2rqVBVA7kH2shiTHx1CIr/cJv/1qw2GChXjP8fQQP0Kyauq9TG/3W8EW6/iNZAQfjpWqSSealfHKSYX6QvIHu71Vc3DdetPT9Vs/QIkaFTLwiKdXuCKZH8m9qluZ5sWWYIj3CKIGeBYhXS4eZzozHqv95LfVULXj/weEoWdtv5WHwxIN8T46Sc+qAUJ8VbeGuvYT2ECuDxmQRDySVXOpN+PRxhM94inqeHH3FUgl9DVLO9CcGhWr8YgniHiciaKdqNHZSmDrMYsbb07Ss2rEZxzEb7T91oCdgW0ZKXzLlSDkQxxzUn8kAa077GM1RAvXAY5V86Kd6IY5aPm97Bh7xHtz8qAOZr/b1SCIxWg3Zzce8QQLV5yNdsVEt14VzEfd/Yqee8R7c/KghIEW8GhrdUr5eO4LV3qqBsaclM89/Uq7CE3U+JABRXMyhDEni4m2bgWbXKkR73d3EKT46FcYUdPXJeMDLtBEjQ8ZkEQ8zEUlqTfjAxZ/hkO8Dxl4c/KAhEGmRzxHynj6kAHVB6eNqcx4LcUjJd5ef0n7/uwZXs9IgQAAIABJREFUPXMSZW1psdQ6NSzAEzWlb7lSRDyKOWkw47EGV/ZP7hHvEX+QqNE7NXCixocMaNrxIDJ+Qs6pqVuuHvHkzEkQxKf6cT9Ep6aUSx8yIGhOPoDQpT42GSCKGh8yIKhqUKgzmWjX1MCdqtorMd+AosfxQwzEj29MTs1D6RHvH3fseP0hHMBo/L7l6hFPD/FXGGyp3WGgTriWkIj3Vo2343+ulXOj3WEQsLPPgIgf3/iQATk7XvyGYXroRQ0XwzUg4nPP8fQg34Pgzvpkse5lws37+ZYrVVGD0YDKTU7N2TUg4MvluUc8tYdjmH4mp4ZDiprSt1wJ2vEQiK9PP+m+isILRFFT3vuWKz1VA0GeaoW2luIjSFFT+gYURcQDNHaksf109gCJ+E8e8fQQjyAXTKImABU18pNvuXrE/wxwNkRFTerteIKI/xdCAWiITbKTNSriPccTAzwLAeIqY6OoATuF41uuhM1JBL1w2deLGta5gwS8b7lSRHxsH/GGuyDqvh+mU+NbrhRVzYl9xL+YRI04vYJFvMcQNY63b3TLS9OAK2aKbNeA8i1XWg9HQPxS79QELFphAv79NaDYt0e8PvzbQ8SctB8yeMlMomaE6U2WyXtpQHEWRt3+oH5Gg0FW/Xn7+mTqL7Pdv1X/ScAj/nQNIGr07adwASpq3kMDquLuishZhfHRYjGbTp92z3b9+uT7v5nOpgr9ffW/GfMcf5CouQZFvNszf1yEitZ3bD6d7sC+SnfP9/5IugP+Vv3rY/W/Xnm/y0ER/9E2moxBYQaaInO5AVUTezQY3Cqcr5Krq1eUSC15yVImV8m2Qv4sG9RkD4j4C+tEaXBqMNd2uNuA4rxT6fLb20VN6vl2nSR/kxSTJM1zxfeK7M/REB9eeFHz08+zc4hX1C4Go8V0vj1c7ab50zTr124OkNlkHfGFyanhEaqocasBVeFSifYdtW+3yeE0I5PtVlW0t4M+EOSt7z3KDZkaYFGjZv7c4Hgmwl5VpVZwf9o27WTIbcX0gzgUHELUW9/0ZRQ1IkRtuJblf51oQDHGosHtrkatVHvT77ZMUsX0txmGorcenTROPwUxrIxPHGi5MtGrHcj50/aogexUEX2/a5/nhe0g2dgUFMY70/39U6Tecq3Y/UMtZVbbI5D7j+yQbvPHWXbO3zvi65FuLWzYHSzix7TteBHHynCfr9YtfYnKfHo7GNjV80yc2Q1pFSZRw0JYUVNu6LZcK3bnFbvPlXBPZFv0poz6Wda1+3tuuZ85Nox0o6xMc6sBxcLah5w/rVsfpkwUz/ftEYXtsLC6/UTWmyTagOJcsMFtpWWS9tj9R55/zM4Da2+c5eiknJgaruFvsKJG2fH06D2q2H2qnBlr72uyfap4XtgBve2w8NhwJqGqrn6FBbwkt3RS5cMUvSfJlbT6ziX5zFK72jbiNwZRwxjgDdc/Ip6Yelf0vtoCbEJJFc3bUPPM7sx0Yrj9FATADdcyJdSA4mpKT9E7zNsp85mNeSkmPlrFjMmpYdFnWMArO54M4llc0bsKEeAgXtH8OXtniK9FDdM2XB+gEU/DqhFhbzCaruCGheUma53m7SJe3vcNuoBhrhSmZMczVqmZaTPx38ZpPn/Muu3SvLCK+PSTwalh0RdgxBOw49UOgsFgMd+ivo3jWcvZeRZ+tEqSJhl/Bizjy+cuPOJZNFJJgiRBfQ+T9GnWqpi3OgJVp8j0mmu4xgU8ujnJWazcyNUa+GtSYf5pNui2iXiLS3vN0Xhkb7IswBtQLBwtnip6xwa8Gg2ctZizEZFFxG9uTO0n0Msgr7+wuBzPWWeg9My6JPDIfNrafBSzyvHLvqH9hJybrIquDNeOZ2K0WNkIiv2kstlkbX1d2uR4o6jBOMqmQzxk4cqCsE6+r0syj0wfs5YsG2GR440psiC6Q/6YLvuY8yBCxIvVVUnrkXlLxrxFjpfPhqAw7mWQ74jH4/e63YTuz/zZN34+G3C3Od4878eG0F/Mzz04xDPF7/N1SfLJJ+ctTMBa5PhxZmy4XiB/QPIZzaphLB6R5Pd9+ZpPW2B5iwuaTNH4gP3jGhrxYHa8ml29pcrve2UzG4jjI94Wqoql0ak5hf74Ciw7nqt+E8SwxyEsvzz6lIg9xL9kva7+h/swhP56HmdAiGdxVbCSM2j+RMsfneXtLWi6NC4xiObQH87vOOl4xqLb+bGXirVlzB85cWCN4wujN8mwvUllToJwfKgCBWv6cN/58sdleWaN483epBhia9J/Y5iTauBDBQrcALxi+eUxWZ5b4/hLYzS+cwf9IUoMO57FI1qBgr/C8tkvx3tnRXxtCy+meT/Yq8X7n2AJULhWBD+ar53h91eTcnnOj8fxllRNLWr0hesJNnEhpONZrAZYHcP7bq/H0YSNLY6vG676fgq2NwlgxzMRjebr0sUnnRyL5a1VrhujqOnMsRH/YjsdXxP8+spJxKslfcwpjjc2XAMRYXuT5Yvt5GS0ePpaOvvkR9oyb4vjX4wpMjYC/zh/79nl+M5w6yjB73RNPjlOXt4Ox0tziizE9iZLeWnXnGSdi6R0+Ukej7LhwBLH1xct9SYS7p3uV3u1a1fV8LPrK6chnz5mxzBs7HC8seEaQO+b3FUilq0a1hmuHIf8MYaimJ2e6/jGJGoY8C0cCHOSMRHffXUa8VJ1ohrHvBXE18ef9CkyAe5NVrW3bY4X4ejaccjns37zPdfYwgyUNKbIgvgBHfG2zUkhej106XcwUMaTxvvaVuZcjXcSAoHuTSpz0u7qDlZBPr5buw35oqpem0a8jV0GG5Oo4QJdoto2J+uPrteLHfdrStm0lGc2OF4at+8x8Rn8k5Qb24ivStcK8iPH/RqZzhreVWaD49X2Pf1PweEFamE/K6xkTa/nuF9TluOGe682OL72JrURA4HuTdbmJEOA/GjuNsmXSZ79Qp3jTXcSqlf1Bf1jLAAWGfCa5F3XNSpu0KSUt8Dx0txwRT5p+c2ctI54VjuU8cJxv0Y1ohrl+NY33W2N3iTyne5Xoxhhy+queB1cux0pqxdSMsqI35gbrvAyvtz0EDZpV7qm9mscR3wl5XmDiG/71p+8NwqCCF7Ggywy2BWvzvehyiYXerR/z9V4wzUQMbyMh1hkoJT8Xtc4jniZzhrLyrd/zzU35SYDPoSPiyhzEoLjd1J+4bqukZvGsvKtc7zxhqs6MQvvtxUwW1b3uubKech3eWMc3+6bJe9NoobbW2n/1xGPsnNSOZSqeHU+X5M2tdyAidN2vS3joppAnOFnYMcwJ6D4DvJD14tXdVmxKcS3+15tjA1XfgH/6ckNztEzzusQ5fyr44gvGhp7FWG7iJdqUQ0z5B7gv6HVT4GC+NcQ5YPrUr5oxq9pm+PTSc9QgmBfpt8jfgZ0HoTXfah44fg8VCnziWgE8e2q5kujN0lBxiurBufoWVW9dns915d51DYfbwTxbXq58tk8/kSgCCtukI6eccbYe9A1Mm0iKl8hvk0RUdybcpMsIuAtj2EO4vwhN+x+2ED1oagh3uxNshP8/qHMse7Tq86rUGED13VNE6cUWLtLyYwH/gLxT3ymko99hOTkD8KmDhu4XryW+eRwv6bVo90qcmjYN/kBP2JQ58igAP9N17hO8uXjwasN2l3RpPZN6l8xs7E/528jHuta996UF+9gAlAdDzm8BdUi4tWZaz3gKcj42qpB4/jd0OvCeZLP/3MgybNWh6Aue0YZf0pghC29geP4/dDrYO48yR889Nom4ov/s3c13WkjS1Sd/IFu+bysB+mEPXCM3w8QI83ymQTPW44JSdbPgWT2wcT7fsfi345aHxg7wkVLXY2qY/8B2+hydevWraoVfP2JghTtmFWzHzZwP1GWXLckeauIB+coKEQMVBquY1ZNVbz6WfHq+ph3tGzpUAph74qkutMN7O14Q4GjVt3k+Jzk+79A8dqu88rshSfhO92eOKfwvLpn1eyRvOvra7bRul28RtgLT6YzMGJAQsbLy6BDObJHNoQy5a9eSB7geGtZxXt4/ImEjI+uu8nx5WaDsfOx4VYkz+zFhWUMnrRkJGT85lp0EvGllHc/UZbO2pC8vSiZWrkOIMUnIePXXSxcKynfU2PeLyTfiShZCo8/kZDx3bRq9kne+URZ2maxAbMWrIlHYMP1jMSmoY5sZ3omUeY8yU/akLytYI0EN6x6rH9H4eO+DERXAV+a8udfHPdr0lVzkmci+LcdoKzApLC/oIB4laphXodJ3v8FYsNxi2koYWnpXQqPP70iMcTTWatmj+Sdjw23IfkM8VaYNYa3GJyReBl32Kqp4rBB4LxDubnmzRFvJW93hIznnT9b3HGrZi8p7/zMa3rT2JNnwkqTTsIRA0FCxm9veh3n+GLm1fXrUM09eWEH8bCM54IEL0Udtmr2SN754rV5uob5Vk5ybMAtBiwk0ThJLrts1VQs9gsUr+umSp7ZOUIDH3ElIuM7bdW8kPxxhvwHC7JrCsv4cxKI77pV80Dyrhev60HDByF8C6f1Enj83/9M4gmtuo/4qvN65zbio2kzeclFYKEFtYGPuIZ/k/AIum7VPMRrnN823HQnn5UW1ArcTcrPSSA+7bpVs0/yjk95J9eiIeItYA1cRuYJGqImJmDVoMRrZPL16/duPaJoOWhY5Yz/xv82gt7k2TsSiF8PRfc5vsoamBxhlpvZ4tv36I8uPaW42YIyG4b8BhoNZWJEQtRsbylYNdXMq9HtNclyNv64uPrWoWZu1Og0FLNgyEsl4wGOJ+IsdOnmGfBYheHtNclsGITjxdcfGdN3pTkybELyFgx51ZkHAC9oNExSElbNTtcYnfLOdM0wCEajj+87I+iTRvvkGb4hH18DiGfsjIaVlnR0O9NBkjf6sW7mGeQz0Gfa5kcnfCC5bBSTFwE2v8Jniylc+Mu/u3MqiC+LV6N51GgzD/KfUSZu7rrxPBo8Di58ZA0t1b0z/vzfQGQp7oSIVbMz5c2SfLIcBSXmFc+f/rUsV03CNeiIT6agjPcJHMPZFmd9OBWO9/L79UatZ5mshkGF+fHV6VlKxk1i8kL0cZ3BDeTGZzL+8/YF8QhS3vSUt6pe/RLyuW9zajmfzBo0BJkf/vdFxh9n1UzpyHivXL1qtr8oM+WsFp8VP/33X078bpbrHm/ABAGqIS/hs8WMiIyP5yQyBo8CZeGVUc2aLrPPoMJ8qOT8SWleNqldM8Sj2pMJfBqEiIzf/p9O4boXlDfLw1IFLR54PlycFvLpqgni/U+YpesxbjwNGb/9PehxSoDnPN9DafTpZtXrQEFe+CXNf3x/ygFDNeKt/UwE7vCAOzI+L1w5OZIfGRbbcfbSFmyH+ewr9SM6Hebvp/pPRPiY6UmHZDyhjME+yQeG16JEm2mP8Zznd0bl6Whe3ur3XQXq/JE7bvz2ft6jJePzh5vpGsN4lMusXOQef8D8+Op0ND/RX2qAa9bA6/f4GZH1z/dDcojnuSdvWrUmywHnnOWY90/dj0putP2z7I9GzHHdwjL+DREZPwmIiZoqXjMyLTri2TDjeM4faD5UNH8qWaNfuiKaNUds0bZ3brB94UpO1ZRK3vAnHG3meaBFhXcqyI8WJxLz8VzXTmCYZs0RMv6MiIxPVxQ5PlfyI9M3LzMp/1sG92IfVIn50eI0wfkGKXlMsyaGQzWvicj4lGDhWir5YGz6NSqLDe7cK9R8WcCeZsphPdAleUyzZhK4IuPzwpXR0/HFNJTxw8ab2dDLgcYfCtjw49UpIsSxtluDONwNy3hPENmrogpXghxfTkOZr5Xiefb9zyG/r+ZPcash0nZrsr8W63WUgjJe+Bc0KF4dpCUo471q5NX0E46WDxUj39H8+AQ5G6m9gjL7RBZIiIdvWtKR8SuiiC+WDZvva2dSvoR8ppy8nbJZ2C9fM1nDNRGPZtbAMp69+UYE8VOKMr7UNRgkLzezAWMV5HfKJhPztpVNcqPr1gg/vEPjRTBGRuSASzykaNU8BOXPzRsE8XyHNF4om1zMZ8LGMs1rN6GEQDrQAct47p/TcOMpTXVbIvlMymdygu8bobmyCa1HDuK/dIW8H+KUrip7Bcn4T2QKV1rheBskn8weJqsZY4UtlCfL7Ir5aCp0OR5pZ80EDtWcUZHxM2pRYQskL+Npz+OPfg/L69cR6pBRbRGtK+RRtMURSRRGRcanc6qFKybJy/X8t31XiPPKpbTL8hvNNcMZA/QxvpPHuPFUZDzhwnVH8gjzzNFywNmTfpewz/KR7vUE4aOUrsfI+CsiiN/QLVwxSX4bz/YFBVMd2IrlLSYOypiPDsljlK4S5kVGRcbLJY3N8c+SPEprPU8bPMJ8ZczbJDPNtqvaWYNQusolPOKKoqYwED8LSCMej+T30waVZ1NA3qZJGWvKGtV1NY94WMZ7/oII4pM5ZRmPSvIPaYO9b1fJ8tYyNlI3JJ+Vrua/j+k1ZG+wV1ROMCqBRprjC5IfI5C8Shs8/k1V+WqxFaXrTyqz5pv5TwKS8YydfaGBeLmmXbju7BoUkt/Mf2K2kuX71pKUsaYjL0R4hVHsQYXrmIqMXw2JUzwiyau98h5/HNe0ruWTubaQNx5Tl/CIq6BySh3+XyiwPBbJb5PZE3OQ5ca8cinPLbG81Dz8l30Yb01/FHCXkr2icfDsmJKEAMcXJI/BuT/rmtyk9H2l5e2sONAeCxGBcSEfX8OhmndU+k/XxK2aByWP4pLL5YjX/DpRbB+28pBjzZWrmZA3PYsEy3hO5G7xMf8LHSWPQvLJTxeZCpZXkD+3Mrqf6OUnGfNDw7HdvNgDUjVUZPz2xgEZ7xWnoZBIfjN/qqN55dj0rbC8XPU0/UnTPah06srd4mNuMFORNWqtAcqHXrPInbEyYxPaSJXJib4jb/Z1Bx4884T/jgjFJy7I+Mqu6eOM0iezYa2O8m15lPFQ7xlliDf7ScCXEujI+PXQDcSXaw1w3qzx9Cc7i1csb8Owudd05LPS1ayQXw3h3UxUZDxcklCya5BIXkXKDuV5QgsL+tKVloGs4pNGl4Md03/6RATxTvSf9iOUOOiry7bsdjct0HWNXOs9pAzxRje1JaCMZ+IDkcI1uey5gviccsc4JC8300G9fPDV5mFsyOse8ObCN+rIr+EYWUilcN24UrhWJH+BhL51zSr3ao9NiF69agt5s478ETIe8+Ca5ZKEGsnj9ITU5Ute+zYXuWGD/EpPdXtQIrgwKOTh3UxkZLwr/ae9PBlWIyTTNexnZJV5A+xQmVzpCnn/rbmvPix9mf/5DxqAd0jGV/vkkUhetV65d8iWD8fIQ80Tva6rWSEPS186Ml5dHffc+cGLGmwfbynbZ/kC8p++o5LcvXYPyuBCk9shONQ9JnIbZLt2SMbvlDxWDRXXi+kyR4kbK7lvIORNKWt1qduVGFn2v/Q4d43ksTpCT1cbPDZscKvXVPNYSIZ4Y7SbSd8eJOOpxMiSqUMyfkfyF1h8k66Ed5jlUZPD8lbvQSkhb0pag9MgjFG51J3LeKcQn5M8WttfbqY95tUaNj01H/IFkegmmlf/lCNv6M+Bo1eCkIzvCc+tH4FJ8io3XP9myatXzIBNPNcW8qa09QqS8YxfEFkqvL0NhGOIz1R1L8BjnGQ1rJHTvGj3oh591SxdDQr5Iy51CyKXurNyyDVRk5N8EKC1Q2Q91fJyJOocT9folq5K3n0w9HYB+09nn6nIeNcK14LkVRcKDfLr2uN75drhEC9Tplu65rdCjMiaCRwj61PpP03ck/GlcYInL5L6EGXZex3/iZY2+F2v66qEvJnOxC28jezti4w/ZdQgj8mjNUTkZsoPVa/5RBQePWnfCgnfGfjiw9KX+y8y/qSyRkEe0TZJVwey6jnk8XSNplmj/Ekj9Qzcs2FnRE78ZeU//W1k9foiCBEvEiV1pnzRe/UR0wbJVLPrasifjOfwbRAq/ScnZXzJbiFelvHxSbSnvQC0Se/0Rrd09f2RAX9yDS5bF2+IyHjpooyvFDVmsCuqv8xULjcIz5Gev2ZEPv/itxd3MEqYeBvRQHwKR+LI2jU+zka+XfHae8YnwpoBbHCvPmh/pwYu9tjrCxqAP+JcIWklj6kt1/Uhl2JnDtavXmtG5JWQD1v7k8f0n6jI+HtXZXxB8ueI4jKZDZ6xRsMFSspB9wJa7k+2ljVH3KZ/QyRGJteZQHOT44tlTZhzeJvrej1YuPI4RUR8qY/49rJmAhZ74l9UZPwqcBTw5cP+hPggouUhU54VrVcEyKeXul6ygTQZjBI6/Sd3ZXwhYYM+pmd2wJSvNstjpGcjXXtSuaVtZU3qUP9p4q6ML3yKEPNJpOs5957TNXfmf+dNT5ehlKxph/j7EdClZHRu0986K+MrcYFZu6qwQS0SytYrRh9K255Ur7qWb7ojZHyfSP8pnTos40uSx3TN1EU075nWK4JfczvQbZ+0dmsyXgReLPwVYqDDbOXvsIyvLHnUWTT59NLrk9ar+T7U+i+h/Sm0lDXpDJz4oyLj5dppGV9o2D6qUZysxDNOEYKuibURr950rUQHbG9wKv0nuQp63GVVw5HDNXmirP4TLFqv5nPD2oZ80XZtg8jYnf6T6zIev+/63Am+AvKmTflE25DPZU0LnS2PKVypuPFgBtQFku+jvnFlfCBRVk69mtY10aX+IxOt7ndLsP8kzhY0EO++jC8MSuTlcAf9Qo4SNvifNuLVh9CiL3HEGdfXfxJB/MzNbPxjWYO5uaYg+VltUt4r18qbDhvcDLhdbXd/xPwTERmfzF2X8UWOMURmoMn8MMn7xm+irQe8iaxpPA8mY3j+qX9HA/Gx8zK+4jfcB5LUj0MVv9wPRldGf9tG357Mm1BN1ZUEl61zKmu05dJ5GV+K6THy7qBDnVdWmvJGST5pgHi1lbDpOhmlfQFepLJGO5q6L+OLNzp27SqXg0NJeZYXryYjy9F/RJPvfRA2XJKXgv0ncUakcI2v/2HvapoTx7Uomv4DkqnJHlOdPVBJ/wEYO1tIkZ5l4OV17+l40vsxnrfXVJx/+yxDPjvSlS5GliP1vguIr4/OOffoXg9IjZhcc3Ra8yBzKPeXXk+bdCj5HDFtRdAaZNzifhyDg+M7IlzzcewBxNc1NzzyI+HFtbzzGjU82QBh1tTsChe3qIQrvMa1I8J1U/0U4kXJR/GxF43K7rzuWVWji9gyRMXjtWul9qAYWVfWuKbz2ANSs2+yH32ZdLGWM/mo2WU5GHtylzTAfAlYuJKoIzS+8ILGP7rRx34oZTZSmfJNXofC2JO7PwLmpAGFK+kNf3Sj4rPxwAtS09B1frgy1LHh5gxSjD25vw+G+COAG88qGv+zEwXP5154k49916Nv5ZreTiS8pi754bfGxOsUhfF1pu7SHIw/Tv/p4234U+Hb8WnNQ7lkso8nzcaG56izGdmXyMYQjWcd6T/l3pAaK0mD3d0QabGJeE1jZ/9igIAqWs/hNAZ5Dgcno8tOVPx0Ew9Yz5d/4kQ/epekXA6IpNiaBfnNCN18Nt6ewFfQ4A42/NkJiK+9SdrzhtZEw6OfvWJVjjRewxp0KDOMPbm/KmB60hWgcKUdEa65D7nJ17Tm6BNVSlmGkjTbhspHFPtHMK74HO64dqP/xLOxR6SG2KE18i1N9VqqxvyifMSQtMZ8C5qGcP1+0YWKr1cwe4TxgtZcHf3J8FvZKr7d3ZCGHEpcC0r8EYwJCCxcWb8ba1z9abi+aLIfH4vSDVPpxoaOmeQai/HGBGT6UWg838YR6/n077Dr/PoO5UoF8g21oVIsxpvbRQlk1fQ60n8qF740XF+40UMLk+LSDe0pTPlmXjp+jeukUHMhAQrXrvSfkrVXNP6R1kwtgPxaBvLi5l0zI8o4YmRNXZzGcMw3oHDtd0K48q1X3uQTrbEw8Zln6qB8EyDPUU3X6qU3v/cHJcorptQJ4Vp61XB9cqOHVxYqXhaU318AbKIrgKx4Yj58Mp3DwclO3Pj74EO023RrREe0J+28Ro1kDbAYb/62gZYeIWddWJXAt7FnwtVatka5RIE2dOWVo+79UcSMjS0sXDux8UxsYPau4Gu3xsaUc0U91nZNAzvjUcEazI5heFV3/3sHCn7n1PR6HtIaK4Ak2QD4GJQ/HORxGM/MT7jpHCI17LQLwpXn8cDDihe05rMV0ilb89oYyN+OMGPJzF81EbCFtEEXhGuZxf5cBnnj1lhZ3iIFedqMXcMxUTLSN6fxcFSYfelCxzWZeEjj97TGSoOQ57Jd3rVdMzkU5FEVT0/M3/ZcY1RNB4Srr6SmpjV2vLR0wZRM/lAmUCAqnv1u/qnZxxCu5dK3FNkTp4ii0x92QGWiYvIHD69BVDz97YtxxqLcQNDITrtA4/3L1LykNXYwqZSB/I7JHxqwRVQ8ZuNqsgBpfBf6T/6SmprW2FFa/HatYvKHgnxV8YZPkGDc+BkkXCnrwqruitQMmJ8FL0i0pRgIz6RM/nCQ5wiMZyfmaAxHhaMvXSA1Yz+dmkd/8tIKKvFEOrymgXRNYozx9DdzNObbeEAbPzjsk5rbsbekpl5abynOXWYyT54e7sknxhiPofEczBiwkzv3K75ceOrUPNIaS5fU5CBf85rDPPnUFOMrNDavTQ3h+tl94crrcZPeYnzFKCY/LUGLfAEgiQ6cnlOOTHvmDHEbZgbdcWVdoPF+k5qa1tgh8nKQb6DxWpqyGgyNf5iBwvXTN/crPl1469Q8qkZbfXFgQtkhN16NMR5F47dwx7UDwrVY+evU7Eo+tjVfRcnkD7NrSkMej6LxpYZw7UD/aeNv++nJn7TVGJev8qYHgnw5NnyIGBpfagjXqfMFn3oO8TWRt7anK1mpQd5axaNoPLjul3ZBuIoVrpR6XfHW/EkFyNee/AE3XquKN6XxxxCuffeFa7rw26lZqOUfAAAgAElEQVSpG+ORvUGJybUS5O8sYTyKxvM/4I6r+/2nxHtSI/KT1og8APL4GWmJIath5+bHCc8gcOyAcBVD4wd+Q7xdIg8weXS43DA7iaLxQrgCM1bPL1yv+HTu5wXXt0EDe0stpkq7ZvgNB/LcsOJRNP5+DVg1LHI/KpwHUlP7kxbnPxdKTx47atgsLSwijua1+e8H6Lhy3834PbjG9oi8GC0jBfkIDfKFWc+VIX4wn8EdV+eFa6VbI99JTe1PDu0ReQjkf1jB+HPzQ63cQnzAfeHK83EgNXVyMbY4cgICeSyPNzJkEb9XCFe1Oem+cPU8RPbiUcU2F7moQR7XhcqNlCvF0HhQuJJPrgtXPhO6lYaKr7D11GLFAyCPiRpwsyl8BLNU+x7afkr6V65X/Dbo1kezZmhRc3EpyNezazDTmqqXyORJMgSNB4VrBzquyTqQmifpahOepGMNaB01+I6reJM3/AZR8RkUwHJ+OJOfWxJkPagbmweyCuRjzAAdM4ynmKXaJZQxoJiTw75uDaRmT2tObVZ8uVSBPGKAjoi8mFg1iBHvYDieRDduVzyfBWvyZcXbtJLlUyh3N0OMK4cvTCqeYbKi91DIlkZfL5yu+HoSGQ0Yv0+EDK2qLukUyh4O5I0qnqCWaidwOP6H2xVf6dbQb30pXS33/lQgbwyWfD4wsWoQ/ScxnFT9EazvtnAts6BbX/Wg7G6oK2VLQ2rtamxQ8pXBcc2iS0TFw+F4x4VrbU0GTvPMJez2C+UgL0reOOYjKl7fqsGMpALD8ZS5LVxDpOZNmcWndlNQ99LNUJghlKkRxp9hhCs0R5swtzuuO2syYPwzsJ7eWYackQrkDUVgYoDxFLWbLAEyBgRledqG+FDwL57X0PI0rUS2yBuzQKEwuOZKMP0nnkOfwDBZHYsQv4zD/dbXuNq3PWpFscg7Mr2ikutXPA6M+S1kTtJzlyueF+Nwv/WtWWM56sqThYRV7i68Gn0bk+gkqv/EoXA8ZTYD1xiID7r1zQOLbA+Q49mop+hC/c/svNDHL1RplqsYEq5O73FNVgHif80Z3Fl/Cu+DPBGXssxkxUYf4xmqNFNocjzBmPzW/k2zYE2+03W1PQiabwbSLxOZDWsyCRlEmHWeBUDjCXFauNYQH6zJN2pxaH30RH4teQg1rTE5cub6Fc+GGBcRtmpcFq4B4t/vQVm/pTldvA/yYmeImXZd6Vc8RW3zXI47LFx5gHiJdLU9e4IXMpBnZnM8Uv0GFC44yeE5Bt8vHIf4oFt/oTX2L62lC6bou+qHawr9kAFFCVcOZwwcFq6FGMsUSM0v0su+dBUjCKjs25iEa0waUH2McE1XUMZg+Gdg8d0za+xPTZRGDUQX6lT7Dcz0K56dYN5r8UrRrgrXCuIDqXm/6/rlwj78jBQxee3vszGwalDcDXylHBauPEC8tAfVwr5p2VQD2jPRrpCufOWpYH4ktOTP5Y5rsQ5GjTvSVZgg8pi87uQarm/HE9xusjkoXL+7C/FxiAk703VV3YUymEGZ6tvxFC1c1Reghq6G43myDhkymd0QxS0AValMUOq9goW+Hc9OMOmhBHql2PkPpyE+lLcEVO1LVznI14OG9S6G5GPtZ8p+x9B4yP2k9MzRiue5YPHBqJHmDFqY+K+8C6VXSPrmJGGfMS91BpqTmIuEVnp8AeKVOYPTyzaO3fcfCKn7rjq9e25gTkaonsOmqxkDnk/iOBg1TknXSlpNFAlKHZt7Ote+7MD6qOn0c6jj2v/qKMQvglGj7kG1AVVlpui76nggqb45iROu4gPUpMZRq0YMMBgEUqPsurZAR/lsLE1QxjoLqoqVvnBF7SYrVhHtpFVT1BDfC7pV3oNqJR2iTFBqfCP9HBllZ5gvCH0AoV+cXPIn8gWh3aq2J1s5ncvtSGbJRxqZYQ46Kc+/sI8SrtAHENaCratR8MU6sHhIusb/aePRJGtV3xUiWhxyUl5WPEq4Qh9AIyczBtOK0wQWD7Ca2HAWWFPadSQP10wuLxwQrkd4kY5e8Lc72RpYvJLWtBN6nUnDNRG8XLnQT9WQE8xMHpExUFs1Tq48S2uIDyzeRelaPZz3E5T15Jrzv5vKGBDcyPAc8oLYZweFa5oFZ1KL1rRjLG9HcmkBaFexlVeXxn9CCdctOMfAQeHKi1U8qCA+cBp177CdHpSItDLZN4qARWhGwhW1+gda5IrM3B/ZDViOw0oQrR5UO0R+mg3kfVc1S06155Ehl2qDrxT55J5VI25zC2cylDwoXdsh8qrJNbF6n3iy1k/VoDqu4CvlolWTBytel8gP27EdUsl4MgpehRIVr91xxVA28ANI/9vUrXrn6Q7iA4nXka7trOGVTq4RIK+celzqs5oIdf16Blo10bljIM/3nCYUtA6Rb6UHJb8YIrSrcgQlz3QnaVfsA2VOQrEdwoY3dy6hfB0vCM6kNpFvp+L5RtJ3recMK2nNSu/RIoXrwxa6UkGrd/LGpR7U/T5eEEiNFq2ZtPTs8msqPXeUN1VS6TLkRoQr7H6Kd3Jyc+eMJ1+T+NBtdb0H9cAXjEgt+TN1+4rqnV+o40tDJ1Rf0Hg74RH/kHld8EG2alY8i1uarsVlu/9qS74JWhPdoKwajYoXJX9+5QbKc3ENJNzmdr8HVZWWypL/quQdWnjGcMJ1Bt+wIuKyVoXyTqRrxFa/0Hsykq5tBQGny4GcJyst+VyL1pA+6ofN4BtWtAZ5weVdIPGTOMQLzCp+2JK5zHPFnGHla5jOmc4v6//EYPxWY/oF2ZX88OrvtonNdG9MksDi3SfyD6nSkldLAJ2XGSVc9YJqTyjfsklZO/EVpwks3qji2yLyPFNY8soIerGCaSvBCVetoBohOypfofw/F60W/HLfbA0Qb0Tk26p46ZxhaGGIbGngKxjGZQzu11qkmD6hfJvyNV3ujMkgW41APh62dTSXEku+Ig3AskvpqpGXwhVl1SRaN6xI7xHlT6/aCxykt6uQLsDQGnU696ggL4FTUU3qAOU1iGoMt1R7Fg+0jCCR8tzJ19bGDNf5MUHiA6UxdeRv2qp46QhKiNZoJA2Qu8lmmoNKiVCvOy5/c9cKs0n/2qnWwGi6Q+QfyqVisrbyPiJoySOXavNtrF9Ce5SPz9vovvKZKPhgTOKIfFudFKl2pdAseekU+udqRAnXMtMfDfLs2PzXfuKg3Ns0QbXiiHxb2utebsmrxwzzbABV/OURrZrnr7lDeftSKBFpmkGQrRhWI6I1bZnKPGNyS141+FicDlT5q3AO1P1EfxvDCy4fn9/8NbWO8IPgxONKPoramzg0k9IaFitHVpdrNRTTCUq4/mtA419x+eHZ3dQabkyLPcLTwGlQtKa1aI24vsOkTSjlqCZp2Pjx/+OE68xspwx5NuYtknleLCchXHAYkf/2f/bOpjlxXAvD6P4Cy6mZvUUl++AK/Qecwtl2Bphthwsz65sAmfUNDXfvrqb/7bVk8xGCJCxLVqr8nvnoVKdSoZKX4+e8OjrHl+Kz7zL7WzcGdt0L1IWrc6vmpH5l/fGmkTSfCKQRTAOkMczx7NrXuWF2PzmfUwPd9oR0pDp5MdxNVsGqeVe+FpKP++MG8DCZT3rI8LUd+bfPhjWFW6PCmpXicJSQ0OgyI7/yR0yekiXMz8abN7dpPkuLopUzPJx4c8X7A3kpj+smUMoPbIUGzQrXn5PqOb7DpbeTvPM0n875wRMLeW8BqtY6IO/Ln+Sdv+edP61bM1KI07DHYNAzYQWuvX2a7w+XG2eMmKwLpBEuDQRfC+S9KT6VzzRgyoFp2f2t/JduZtWIrpqOkeS5Mx+xQvPu0nwqTBp0j9kAeX+XeeQzDTSDtX/KOw0MC9dsZT4UgEuelpKfjl8d4HyWLkcCaURHPCRfI/iQLX8DogdPipkGL0orUZrpSGi00i1bREY5nv8MycGzEf2U9slmWyA8XBorIM++eMSaQObWhOpDKPk6KdI1tmpqJI5gB/OMxbPh0m6eT9dDgfACaeDS1KQaEoZ3/m6vfTd0a34NHmUgb1i4bieROSCTzrs0z/rT17cks/QzekjWI7YnGtSsNkDe34Do9TeJW0M0Uzwy+SrkOzPF9+rk+Py9IjybfZ7vz2z5Npzge6Xgqfg+iNqOvD+Q3yomUKrXoN1LGigJ/WJk1Qz005nUXMP/OUieX/zOa9gkeaiFN1mSLEVfQdkPj6LVDsh7xBrVIZSykXkrKQGCsHGrZv8uFRdgS6OSdbs80Y9f68z4yNbzUbwXfJ4cIHgrbo36tMc51kjdGnX/gwRryJVR4ZrNWb0cX2he0Pwe58sqdvOW5/qkMuIkyWY9n/XYEdFA8Lawxh/IJ4qlUOqZq4PzB7b0xkzxo6i+4gXcCNHv2KbL4jjuT6fD5XJT8UGabeajSdw7qllRtVpza5i/RgPVIVRXOWghfT4704CaPbDqmZNHFSwXJj2wDStbbmYCcDYXZvok3ayXwzK/iwQPtVsG+f/5xJqOEdZkZ2caGM4xEOakrRRS4DwNQ8bey346vIzqszSXe9wr83tUuPAgGotYw27+8Kb4raplWElbg3NfaTjHwLCPTAJkpGw8eJfohWc5HY5zrt+8yXJ9zu6b1+V8OJuU+V1UrBSWpOXSNfSzsF7dMtzRbcj59XymBCChkdWa1TUnT36kQdlg9iHTi9aboQzqs7xWHe1gZgc0vDcYCd421njcbLS+lRnrVL34j49qCj48GcyWamd8r6XNRMp7bcpET08kH8ezWV7L5sUsjw2PtfhwOByOZrM4Pug9ggnvpnTVXLJzjDUTamgiJWe2J5haNXOrOf5g0NP8jSsOpj7k+hzs85gNRcz4xyefD3l652PHIHgn/qRPrJFPGVY2uZ0DImo2jSR7jKw3JZLyUKo06T8CTpdHLIJ/9O5zUcEzuOzkCGt8+pPZ+lbu1qj1+9HnCahZIyg3J51oKyj8yoCKbH/UhKAOWvI7iMYd1vgDecWGHPWUYX58RU4L1y9maPUUuWo8J51yts0+10cqrRdmZOFHQu0uscafP5nJFv/x+VHqlD0/XQoVXJl1xQ2eXF61KPoDgmCneSKTfVi8JfYHrEAah/6kR6yRjcfWLk/4eIeK/G7WMLF2ULie/pC55ssmhDLXhzxy6Ys/aUSP9A65O/cnPR67DuRuDbt5rdSVQw0v7c7dK35v33TKyU50p/3De4AEO28GXQXOFd/11z+p2AmVP3t0j4fAglXza9GQ4nedNyKHC1YP9hRDhNj5xGDweyNujUd/ch7J3Ro11pxUvaZWjQtzUsM4wc7FKXI+T/6BaMmB3JspXf1ijew+k+7uH59AQCxYNdnXqPkpMDvjcad6kHuzkvd5LUTWTSZMJGVJnb0fM2xq1SRPka+pGAB2XyDv9dhVOn1Gsy5E9CiQ+lZN+oQ5MHBrmlS8bPoM0T573nca0N/NCtd1Y4Ur4rNQDQmZv7Ha8qF6HGv+fXkNQH8zG5gx7+GSUftAPvS2vztnk2eJ4gPdupB3NQChhnvcFsjxrUzyd/5A/l7lT/6jtuQPX0quzKyaX48RML6NIO9xyPAP2fo+fcvP0YktuTLbTsvNSeT4VmLN5/Mn9VcSt4e9sPTKbAO5R3MS4S0C6hVrvtPA0ETi2xP25qRZ8Q1zEljzafzJC1p+fu5WBhJquLcQ5mQ7S1d+vvn5+ic7WqzJFnT3mLoz++ZzKL6lIM/uEo8gTyQvi6onyYvnQ1AUrobvWJiTbZW8T6yRLvDOX5ZmXWVZ9ppO7uCDb4Dx7QT50OOmy1VPOmRY18m8255gatV46ZxEfAbJR/5W1vNNl3KQ1wyQui/GDJt21SRfkePbWrtqLtk5xZpnGcjrB0htxUyDgF6b9cINYE62t3b16NbIFjuJl/VywdeSfxmeJ6yeULi2M8QhlLcGypVst+sFkxaEt0muDKuQFTon21u7Mn/rQhQgr+3dT54jYdWgcxJRGWu8rTOWgvwFp8FipgG9MvRWYU62GGsou375dCDf0V9JHDxSQq8Miew/DOZkm7HG23JX6do/ceyqVny2uKXUcHYmzMl2Yw0zWwBsA+S/UunL0uxOyKvPbzQ0tGpSKL7dWOOt0yA7v7yvc9HItOQxMrVq1jAnW5zjif6A010sVCCvk/Pi1tSqWfVwANVmrIn8bciRg3yONTpkWX8znZz5HeZkiwVfWPIPnxDkb7RYExsqfhFB8a0meY+162Mkf1Xao7HVzPBlw45vOdZ4nEAp24HGa1dtx89mafZoSp7RK9zq4OtHPe0zli8zJiQMb3SNzBuzV51iWE27qYa7NZ5q11x7gQLkdY3MmeENW/QKI8n72Z4g9mZLFe+skfkeVk3LQV7c8PZRuw5UO1Uv8CeNrxsix7e9dg29GJTpvBcpcrxum7F5AxtDjofkNfMy3Jw/TZiqhLzAnzSL/0ZQPLCG9Ru/75ouekyBF4T7ky4ePBnseASvXRsf4zF4ylO84vJdDvLXLl4TzEmElySfjnosVOAFb61xclllAMUjRHNNs0MNsvseY0rPhDgC+cEEVk3rg3fJN5vks8GjRvDF2j8HWAM7HlEm+SbXJ2QrnuKVIzQE1rxB8QhHJM+anFyTrSc8xROdaXrjAGtgxyNKrumOG5tBmS4Yi7Q4nYO8/Vvn2XcUrogyyTdG8tkq1lJ8cexq3zPdolcYsZNXU6O1Rdka6Qfh8dYa66S1hTmJKLAmDJu6GbJd9C5gmuIi1B+2v/lPKB6xNwO7fzVRvCa8bI0umXXKQd72Y2cwQeGKKECek3wTY1fTR3ZRihfFhXXQusdcYcQuo+YkP3Wf5NO5OG0NLntFd7ZvZ93DnEQcJfkGZmuvnwrBXyR5+yAPxSOOUyqbOi5es8Gopz982ps11kEedjzikOSJSPJuHcpkcTHTFO9ByyMCswXseMQhpfLi1WmST1aTomwNLnxFtm/gwo5HHOQlsIb97bDXIFtf6tMcsOYfKB7hKIJO4VC64xp+DSSKKtiD2mXGlQ+g0B2POJI8H0LJ3DmUCe8RrpDiC0feKtb8RK8w4pTk47GjJJ9xiOdd8aTCC7I8L+0HzEnEMcmLJN9/fXAD8SMu+EpUEeiXGcOOR9QJyttrpg+uIL6i4K2DPLfjoXjEUZInIsm7uBuSzktjsoodbhvktwtYNYhzkn9xAPE9wTSk2vmPZZCH4hEfkqo4ebXeNpysHoXgq/YtErsgDzsecd6vsd5Rlk6MBG8b5GHHIz64I2J4jWW/JluPCieedKpKXr++G3Y8orZfkyd5qzdM+VTVCg1kpyBvr3T9gdnxiHMkbxnl01Fh0wSdwEDxNkEedjziHNgIv+ZmbEnyWTp/YrydhnZM1MbHTz5YM4ygeIQ0ydtqKRNGPAtNccLm1BrcB0Eo/Bo2tVG9ZoXgIxoEZjeqbfbIb3EfBCHza0Jevb7YEFnMSpvG9N0X3tgqKWDHI+QskUs+/mvzYCnD08B4ZkYO8rbGA24nUDxCgfKsW7enZZ/hA1LjzWdtPOAWB1AIuV8jDqLGdVg+24gMzxsma9Czxc2uOIBCyCUvuIZ161wPSedlhq+VWDnIWypdBxhIhpDqrED5PMtvjFsLhrsMH9TSWa54SyC/Ro5HaCVvukghETdAamf44tTVzlo2HEAhlFxTgk1/aMLy6XwUW8nwXPJXd9bGM+H3ilA4NqJ65S02VdN8khQ1a/0ML8wadm3lUlY2wgEUQoU1pWHD4um4WumYrEeT3i7Dk9oiy0HeSjMZDqAQOskHZZafvr49VCB40Q5ftksatY99APk/7djxUDxCB/Ol5PvTl4cKHk3M9ietFkpFQrtfrCj+CQdQCA3J71g+J5vl5gKYztLNciQQXpw72aFmDvIPOIBCNObYFDDfv6SXMgeaOGZ7k6ZjR2GWmskGUDxCz/J5mg9DoeH+dPyqyvM8v5eepDBpCLEkeEsgn+EACnGZY1PCfI7zs7GimzJdlnIvBG8twxfNZFZG5kDxiAsdm73kp+NlnuiTU3dms17Oh7O4d6x3a4LnIzy+POAACtEczJO95oU7//p2gjPreWnAlwRPec1r8V1Hw2s7iscBFOIyx+ad5Puz2XQ63sVwOJ3N4ngn+FCcOll+x1k5g8pwAIWowDb0SPNC9/s4/tsiwXcCy025ueL/xJErokGsCQKR5kvXRiA963X5f6dy53q3zQ7597ZQuuIAClHRsyGnef59FAVrEAT2YZlQC6VrrnhYNYhKnk0gkngUnZG7+AwNHF0xykG+/lhA3IBCVFd9pxA9fSf6sPg74c8QJ24IsVG6wo5HVBY813SpeRrSKP83KtVeNo25cv9snLpC8QizKrKz55uAFn8SUdk6yu9l6cr+rgvyi1soHmECNqTAFy7DgIqTJl6ruhWThdI1yxWP3x+iViFb/J+bM65LQvJ/9s5gOW1kjcLBfgG1VON1bJU924tVyC+gjGEbDPguE64g6xtjyPpig/dMmX7bK5GkZpJxErtbSPrV3/cAXuBTp06f/+9WAUfX/jEeD+btSZ5g9lFE7u3oysgVJJEdXa8sFf8WxYMgxVsfXTeMXEHSuSE7utq9xddl5AqiFO9bfjhh9Z6RK4gK8pYf7+bOH7h1dJ2heJCUaqynrgygQNrRdWqreO78gaijq11ZwwAKhAX5E7ujK4oHYYpXoZXiGbmCsFhjV9YkDKBAluCzo+tVz27kiuRB1NHVqqxZ4fEgLcj7qUVZw50/EOfxVnsGKB7kebxNWXPfRvEgTPHZ0ZUlA3Dq6HptfnT9SB0P8hQ/fbAZQLFWA6LwrMoaRq4gz+MtrkF133LLFcQdXf2zTxaK9/gFQZjiLXbJLlkyAIGxxnyXjCUDkOjx5vXkHYoHiUdX412ye5YMQKDizbcnGbmCxFjjpz3zkSsDKJCneONCniUDcErxm/+ieHBK8SwZgFM5niUDkKn4aY8lA0DxjFyhoYoPTGeulygeRHq8qeJZMgCZHm+6ScZLBiBT8abbwvcsGYBExRvvx9+yZADy8JTxE00sGYBIj49NR64oHgTSUqZLBttM8ZxcQZ7iTV/v6PKBEBCpeOMlgz5rNSAwxqsPjFzBIYtvqSsUDy55vP9vw3LyzTmv1YBAkz/5xJIBuHRwjT5ZeDw/IEjDMx653p9Q1YBAjzcduer/sVYDEhWfWiie3w/kKd70EyEbFA8iFW86gNqySAYCUcYDKNZqQKTHGw+gHvvK4/cDcaHGWPGXY0auIFDyxnf+csXz+4G4HG88gHrDIhnIwzO+AZUvGXByBXk5/uKBtRpwSfHGn8T54wTFg0DFfzBWPHU8yBN8S/3H9GOu9yySgTzMH53ktRoQafLGH+xmrQZExvj4BsWDS6nGeAC1ZZEMJCreeADFIhmITDXGA6hHFslAouKN63gWyUCg4DPFm9bxvEgGEiUfGA+g/mStBgQeXI0HUCySgUjFh+aKP0bxIA3PfACVK54fEMR5fMdY8X+wSAbyDq7mA6jNPR4PAhV/YVrHb29RPAhUvHEdv0HxIE/wrcBY8Xg8SDy4Btea1UlwSPFnxop/ZK0GBKaasxvN6iQ45PGnN3g8uIOnOktjxbNWA/JSjfq9x+okuKT4C22seF4kA3mKN6/j9Z/HKB6kCd5G8axOgjzUkXEdv2F1EiQq3ricZHUSJCr+dIniwR08dZqwOgkOnVzV75rVSXAI37yO3w5RPIiL8cHUXPG8OgnyUo35drzesjoJ8mK8heJZnQSBkjev43PFk+NBWo4/WmqWhcEZPPVbT7MsDA7l+FPNsjA4hEUdrx+PUTxIi/EWdTzLwiAv1NjU8Zs3LAuDOI8/Wpgr/g6PB3mKX6J4cErxiWZZGJzBU79ploXBoZOr39F4PDik+EOLOp71eJCneJs6fts/9vgJQdbB1aKO3ykeyYMwxS+01YUQfkKQlWostuNRPMgTvDp60FyBAodSzWnPwuPPuQIFwjze71gonitQIA3Pn6J4cMnjAxQPLgm+FVybK35zieJB2sE1sCgnWRYGcTFeBUs7xePxICrVqLMHK8WzZQBC8vsOpVRso/jbE/p4qL/afT+KwihjHEcTm6pmczfO/0yYyR6nh1rGds9TXhDFk8lgPp8v1hkWMV7rZL2ezwaTcVt5pBuon7dnfjzJxD4Y7dS+7Gl7Nt31aj4YDieZ2yuPlzygPrldtaI4HcwXy2WS9HqJLowkY7maDycR6QZqovcgN/dBlmSKMfanZN/NrH4wmUQR8Qaqzu7KiyaDxVLvn+58kGd6wg1UhgqjSZoF9/UyKUHxX5x+HPE2H1Qid6WCOJ3vK8n8SPar2SQ6pqiH0g+rQTTZRfekVMHrTbJezWfDMYkeSgzvLXUSRuloXa69/93oB5PonEUzKK2didPRonR7/8bo1/NM9KpFooe9+7sfRWk55czPVZ9pfhxFbcIN7N3f8/Teq1rxOumuV7NhxEeOYY/+fhjFk9GierX/raKfRNEx/xrYj7+rOF0kSVIbweteknTnw4hNMyhe7a/Uzt+XumZscp8fR/Q2UDAqzP19o+vHZtOdDdsMYqHIAJ/3kfOlrilZtJkNx+f8o6CoSNOK0hqdV59kdUttA4UZ/GC+TuoteJ3sZlJIHqz93Y/i0VJLIIvz0Qn3RsAuz4T5QkFPhOI33Xwk9Zr/GlgUNNFEiMF/Ef1qOM7yPP85MDJ4L0rrH+C/j/OrwfiYphJMBB9F6WKppZFkko/QPLw8wQfpYp30tDzJJ6vZmBksvFDwYSzR4L+m+dkkamPz8AKDP4hHy0RLZZOshqzawPPJlwqWWjI7myfNw7Pw1Nko0eK5HLaxeXgGqspL2wU/9HHOqg38KsKrML1pgN4/a76PzcOvOsnJaJE0Q/C7tYNxG5uHnySaIB496AaRzMbn2Dz88Mial/C9Jik+s/nhGJeHpyNNPnVqlN4/95SD6BzJwxMR/rmSr1AAAAikSURBVDAerRPdOJLurM9XA+EfiaYlbDH4BWzzaRT/YvjW4oO0iQb/Nc3fUtnANw5/mD++pBtLkp1f20ge/mol49FyoxvMZjMb86QNfCVMR2vdcPJ737g87EoaFTevlHyC1ZjKBnLFh+l86YDg9W4Yxf/b+TNr/qCkCw6/0/yQlQMcPmx0SfN9ZTMY80FYtx3ej9KFdojukJtRTgu+FY7WiUuK33R558BlMoe/0Y6xvY3anF9dPbTGo2btBj/P5ec8WuYoQTxaagfp5hej+Pfj8O6QrNg4wOGdc3kk75jDe9F1T7vLisbGQYd3WfGZy/8Ll3cpw0cjl/WeVzZ3LBw4xIHjDv/F5Tm+OmPx0XSpnWc74/lhVyLNGQ6f55r1jJdsnKAVpTj852BzyxMHLjh8MF3j8F/Xynh8uPk4th78c8lfcve18ZEmSMnwf9G7HDJ8bbTeXwXuXPF75orNsI0uGoyKbxD8d5J/Ty3fXIv3Hd4e+8kk6jXSaGqmCa9x+H8eX3mTsrGCZ7fgaZcfUNg0U/Fh+oC8n6zls9Mrmm9iL0lN88PChtlr81qaoHODtn+4Osxl76bhHcTXCdL+cWFDlG8YB/SSP18dHrJh06gM3wpGOPwvNmxYlm9SiA9TQjxR3qUQ36GI/2Vhc/eeWNOgEI/gf316vT3H5BsT4hH8c6J8n8NrMxTPPe5nR/nXSL4Jx9YOgn8mszaKl39qVTELk8+O8tyIakIxSYh/PqsxCzbSQ3wwpZh8QUXJEzbS8bnm99Jcg+IlO7xHiH9hX7PuM3qVrPhw+oCKX1pRYvJyexq/Q6Z5+eiVXCOWAzKNgcnT14iNNGwXGOYaVuWFcsgLk0aK7/JMmVCLP0PwZlzyWTSZPQ33/AzZcutVZk+D4I0l3+ctSnkWH/M6DaW8Q6hwiuAt9mtuWaIUlmlaHSzeqpTvY/KyMk1EFW8n+VtKeVH4XPSzXTboM3kV5PA+x1Zr7t5j8mIEz5cRitkoQ0piMg0vkBXAiutQYoqa8AqLL+LwiskLCTUB3wIpyORZr5GR4mMyTTFwN0SIxXPTryiTH3LnVQAKiy9uvQaTr7/Dt46YthbXUA7Zoaw9BywJF2ny5yiq9hbP8AmTd2r4hMUXbPIk+XofW7H4gk2e21C1xjtMsfiCTZ4rr7WO8Uc0kwWz/YjJ11jwh1O+2orJO5XisfjiTZ4kX+MUj8VT1zgVas6w+L3UNezJ11TwB1xu3YvJs0JZ1xQfcrl1L/AMZU1TvEcXv6ezK5eh6hlquM69P5MnyWPxLpFg8li8W/B2TR0tvkM1uT+T54EyLN4xk29j8nXz+PgTutzjFOotBWXN4AEDplBOZRoWDPZt8u9QfK3gst/+C0o2KOtk8cE1otxvrFkRa+qEwuL3Hmv6vE9Wo6Im4DNnezf5GXPXGqUazq3755K5a30s3r/g7tP+TX7I3LU2Fs+5lbmrW+fWU0JNCWz7zF3rcm69QI5lxJo7hcfXI9QcEWpKUfwlL9fUQ/DqlGqyrFhDJV8HxR+OEGNJsYa5ay3OrTxEVhaP57Q1NTi3HnQo48uKNbe0NXUo4wk15cUaTL4GimfDoMRNA95qqj7U+B2amjJjDW1N5U0NN7pLjDVveGe48qaGUEOscSrUqPQBHZZo8rQ1lTc1hJqyh1DkmkoVHxJqyo017zF5Qo1LdD9y3bVSfEJNydy3STVVNjWEmvJjDUG+whTf6hBqyo413ISqUvHqA6Gm9H6SWFOd4F+FV0iwbN68Q/HVxXie0K4g1vCwdoXdJE9oV8BH3q2pLNX4V8T48pkR5Ak1bvWT74g1VSmebrIKkrcsyVcVaugmqwnyxJqqBq50k5XA/mRVVQ1fcK2on2R/sqJu8oIYX83YlbfJqonxATG+Ilg0qEbxxHiCPDEeaOQba/HqglBTVZAfE+QrUPzh/9s7m5zGgSgIx5zAbQR7d0T22HJOwIiwBgJ7i58DTBIOMMDMfkbDcXETOIKrLPqrI7Q+lerVa7cfQc9FPEHe0cbz8ycf8Tyr7YjxS2K8jfjzU4K83uP5TbdxB8XTZPoYX/G7M+cOijc89PsnYjyja07AF8dPgMfomlOMPybGO3dQjK7qpiac8Pcno94YXeUxnsHVSjzXJ9U6fAA7J/HbQJAXD65UNd7RtYZ48eDKxtU7utZcGNYSf8TgatX/htFVXNUAnZd4yhruGFDWoBHLSaoaypq8iKeqcZc1EC8dXA+5VWMm/gfEU05mpX+BelJKPPfIqCdzivFhcQFzEJ+Vx0O8v5An1eiAx+P99SRv1ihVdRDvX0Hh8cKVK8RDfF4LKIi3F/IsXfH4vDwe4oXAQ/wEPP4lUE/i8XkRz9JV5/HkeH+qeSHV4PGkGjTSBgriIR6PR2Lia4jH47PK8RAv9Hju1UzB45lcdR4P8VPYQOHxwhy/4IsQN/GXEK9MNbwePwHiSTVC4ue8ZWBW+iIE5IXE816Nm/gGj9fWkzBnJj5CvNLjqxNGV285+RyZXKUev+DBGnM5WePx0kKeIG8mvmflqo01seMBeafOG4gXB/kFjbwzxm8YXKUqQ5w/XACeTavLSB0vDvKxo60xNjUNg6uc+PktJm+z+HUkxsuDfOwoKH0WT6hRE1+kJE+u8QD/miwe4NU7qCour6DPQvy2iRXvrBpiTbx7+gN/ct5/b/pYE+M9yHc/mV7FvP9dbZpIqLEE+YH49m5HllfyvnrdrPtmAD7MQF5fUA7Ix+XNrzOk0mp33SeHx+I9sSYMxLft/TXSaH3ft+1w5snimVv1wH8in6BHGsW9BuCxeEuS/0IeSRUA3qUygLxeVYo0IG8zeZCX8x7KkhRvLGzCQQWHItVDgh8cvgB4W6wpP5IN0ikdOeBZmS+LAuo1SuaeIjzIm1vKD+rRyJqVxf6oQc4O/N7q0bhK4R1zn0xnwyGMnx9nnDJCCCGE0DfQOwp9Il+aJO76AAAAAElFTkSuQmCC">
    <div class="box">
      <form class="fill1" action="/get">
        <label for="ssid">SSID</label>
        <br>
        <input class="tf" type="text" id="ssid" name="ssid" placeholder="Enter you are SSID" required>
        <br>
        <label for="password">Password</label>
        <br>
        <input class="tf" type="password" id="password" name="password" placeholder="Enter you are Password" required>
        <br>
        <label for="deviceName">Devicename</label>
        <br>
        <input class="tf" type="text" id="deviceName" name="deviceName" pattern="[a-zA-Z0-9]{4,8}"
          placeholder="Enter 4-8 Character in A-Z, 0-9" onfocus="this.value=''" required>
        <br>
        <label for="uName">Username<br></label>
        <input class="tf" type="text" id="uName" name="uName" pattern="[a-zA-Z0-9]{4,8}"
          placeholder="Enter 4-8 Character in A-Z, 0-9" onfocus="this.value=''" required>
        <input class="button" type="submit" value="Submit" Submit!>
      </form>
    </div>
  </div>
</body>

</html>
)rawliteral";

//oninvalid="alert('You must fill Node name!');"

const char success_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta http-equiv="X-UA-Compatible" content="IE=edge">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Success</title>
  <style>
    html,
    body {
      background-color: #76CDA3;
    }

    .center {
      text-align: center;
      margin: auto;
      padding: 15%;

    }
  </style>
</head>

<div class="center">
  <img class="Current" width=100% height=100%
    src="data:image/png;base64,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">
</div>

</html>
)rawliteral";

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

void popupLoginPage()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;

    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2) && request->hasParam(PARAM_INPUT_3) && request->hasParam(PARAM_INPUT_4))
    {
      ssid = request->getParam(PARAM_INPUT_1)->value();
      password = request->getParam(PARAM_INPUT_2)->value();
      device_name = request->getParam(PARAM_INPUT_3)->value();
      user_id = request->getParam(PARAM_INPUT_4)->value();
      data_status = true;
    }
    else
    {
      inputMessage = "No message sent";
      inputParam = "none";
      data_status = false;
    }
    request->send_P(200, "text/html", success_html);
  });
  server.onNotFound(notFound);
  server.begin();
}
