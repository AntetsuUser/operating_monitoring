#include <Arduino.h>

#include <esp_now.h>

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <Wire.h>
#include <RTC8564.h>

#include <time.h>
#include <math.h>

// *********************************WiFi接続先の指定*******************************
// ############### 池田事務所 ###############
String ssid = "Buffalo-G-0320";
String pw = "cfn3sw44jxmkv";

// ############## 6製造ルーター #############
// String ssid = "IKEDA-AP13";
// String pw = "8286andAPad!m";

// *********************************機器番号の指定*********************************
String Device_Num = "0085";
// *******************************************************************************

// *********************************IPアドレスの指定*********************************
IPAddress ip(192, 168, 2, 5);
// **********************************************************************************

// *******************************ネットワーク基本設定*******************************
IPAddress gateway(192, 168, 2, 254);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 2, 254);
// *********************************************************************************

#define RELAY_RED_PIN 35
#define RELAY_YELLOW_PIN 32
#define RELAY_GREEN_PIN 33

#define LED_GREEN_PIN 26
#define LED_RED_PIN 27

// 送信間隔
#define SEND_DURATION 5000
#define host "192.168.3.91"
#define httpPort 80

unsigned long lastDidSendMillis = 0;

unsigned long wifiMillis = 0;
int wifiReconnectCount = 0;

#define LP_SIZE 5
int redStatues[LP_SIZE];
int yellowStatues[LP_SIZE];
int greenStatues[LP_SIZE];

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

char data_str[20] = "yyyy/mm/dd hh:mm:ss";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp.nict.jp");

// 日本時間
#define JST 3600 * 9
String ntpServer = "ntp.jst.mfeed.ad.jp";

//////////////////////////////////////////////////////
// ESP-NOW送信
//////////////////////////////////////////////////////
void didDataSend(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.print("ESP-NOW Send Status: ");
  if (status == ESP_NOW_SEND_SUCCESS)
  {
    Serial.println("Success");
  }
  else
  {
    Serial.println("Failed");
  }
}

//////////////////////////////////////////////////////
// ESP-NOW受信
//////////////////////////////////////////////////////
void didDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len)
{
  Serial.println((char *)incomingData);
  sendDataToServer(String((char *)incomingData));
}

void setupESPNow()
{
  // ESP-NOW初期化
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error: Initialize ESP-NOW");
    return;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    return;
  }

  esp_now_register_send_cb(didDataSend);
  esp_now_register_recv_cb(didDataRecv);
}

/* ----------------------------------------------------
 * 初期化
---------------------------------------------------- */
void setup()
{
  Serial.begin(115200);

  Wire.begin(21, 22);
  Rtc.begin();

  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);

  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, LOW);

  pinMode(RELAY_RED_PIN, INPUT);
  pinMode(RELAY_YELLOW_PIN, INPUT);
  pinMode(RELAY_GREEN_PIN, INPUT);

  for (int i = 0; i < LP_SIZE; i++)
  {
    redStatues[i] = 0;
    yellowStatues[i] = 0;
    greenStatues[i] = 0;
  }

  #ifdef ESP_PLATFORM
    WiFi.disconnect(true, true);  // disable wifi, erase ap info
    delay(1000);
    WiFi.mode(WIFI_STA);
  #endif

  WiFi.config(ip, gateway, subnet, dns);
  WiFi.begin(ssid.c_str(), pw.c_str());

  unsigned long time = millis();
  Serial.print("Wifi Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    if (millis() > time + 5000)
    {
        Serial.println("retry");
        WiFi.disconnect();
        WiFi.reconnect();
        time = millis();
    }
  }
  Serial.println("OK");

  // ESP-NOW初期化
  setupESPNow();

  ArduinoOTA.onStart([](){
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else  // U_SPIFFS
        type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type); })
      .onEnd([]()
             { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed"); });
  ArduinoOTA.begin();

  // NTP設定
  configTime(JST, 0, ntpServer.c_str());

  struct tm timeInfo;
  while (!getLocalTime(&timeInfo))
  {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
  Serial.println(&timeInfo, "from ntp: %Y/%m/%d %H:%M:%S"); 

  // RTCに書き込む
  RTCWrite(&timeInfo);

  digitalWrite(LED_GREEN_PIN, HIGH);

  Serial.println("Boot OK");
}

/* ----------------------------------------------------
 * メインループ
---------------------------------------------------- */
void loop()
{
  ArduinoOTA.handle();

  if (WiFi.status() != WL_CONNECTED && millis() - wifiMillis >= 1000)
  {
    WiFi.disconnect();
    WiFi.reconnect();
    wifiMillis = millis();
    Serial.println("reconnect");
    wifiReconnectCount++;

    if (wifiReconnectCount > 10)
    {
        ESP.restart();
    }
  }

  if (millis() > lastDidSendMillis + SEND_DURATION)
  {
    updateSensors();

    int red = redStatues[LP_SIZE / 2];
    int yellow = yellowStatues[LP_SIZE / 2];
    int green = greenStatues[LP_SIZE / 2];

    Serial.println(red);
    Serial.println(yellow);
    Serial.println(green);

    // 緑黄赤
    int status = red | (yellow << 1) | (green << 2);

    // 実際の値 | サーバ送信する値
    // 無灯（000）0 | 0
    // 赤（001）1 | 1
    // 黄（010）2 | 2
    // 赤黄（011）3  | 4
    // 緑（100）4 | 3
    // 赤緑（101）5 | 5
    // 黄緑（110） | 6
    // 赤黄緑（111） | 7
    if (status == 3)
    {
      status = 4;
    }
    else if (status == 4)
    {
      status = 3;
    }

    Serial.print("status: ");
    Serial.println(status);

    String year, month, day, hh, mm, ss;

    // NTPから時間を取得する
    if (timeClient.update())
    {
      unsigned long epochTime = timeClient.getEpochTime();
      struct tm *ptm = gmtime((time_t *)&epochTime);

      year = String(ptm->tm_year + 1900);
      month = String(ptm->tm_mon + 1);
      day = String(ptm->tm_mday);

      hh = String(timeClient.getHours());
      mm = String(timeClient.getMinutes());
      ss = String(timeClient.getSeconds());
    }
    // RTCから取得する
    else if (Rtc.available())
    {
      RTCDateToStr();

      year = String(data_str[0]) + String(data_str[1]) + String(data_str[2]) + String(data_str[3]);
      month = String(data_str[5]) + String(data_str[6]);
      day = String(data_str[8]) + String(data_str[9]);

      hh = String(data_str[11]) + String(data_str[12]);
      mm = String(data_str[14]) + String(data_str[15]);
      ss = String(data_str[17]) + String(data_str[18]);
    }
    else
    {
      Serial.println("ERROR: Get Datetime");
      return;
    }

    String path = "/scripts/IoT/reception.php?";
    path = path + "number=" + Device_Num + "&color=" + String(status) + "&year=" + year + "&month=" + month + "&day=" + day + "&hh=" + hh + "&mm=" + mm + "&ss=" + ss;

    sendDataToServer(path);
  }

  delay(100);
}

void sendDataToServer(String path)
{
  Serial.print("connecting to ");
  Serial.println(host);

  WiFiClient client;
  if (!client.connect(host, httpPort))
  {
    Serial.println("connection failed");

    // ESP-NOW
    esp_now_send(broadcastAddress, (uint8_t *)path.c_str(), path.length());
    return;
  }

  client.print("GET " + path + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0)
  {
    if (millis() - timeout > 5000)
    {
      Serial.println(">>> Client Timeout !");
      client.stop();

      // ESP-NOW
      esp_now_send(broadcastAddress, (uint8_t *)path.c_str(), path.length());

      return;
    }
  }

  lastDidSendMillis = millis();
}

void updateSensors()
{
  for (int i = 0; i < LP_SIZE; i++)
  {
    redStatues[i] = digitalRead(RELAY_RED_PIN);
    delay(10);
    yellowStatues[i] = digitalRead(RELAY_YELLOW_PIN);
    delay(10);
    greenStatues[i] = digitalRead(RELAY_GREEN_PIN);
    delay(10);
  }

  sort(redStatues, LP_SIZE);
  sort(yellowStatues, LP_SIZE);
  sort(greenStatues, LP_SIZE);
}

void sort(int arr[], int n) {
  for (int i = 0; i < n-1; i++) {
    for (int j = 0; j < n-i-1; j++) {
      if (arr[j] > arr[j+1]) {
        int temp = arr[j];
        arr[j] = arr[j+1];
        arr[j+1] = temp;
      }
    }
  }
}
