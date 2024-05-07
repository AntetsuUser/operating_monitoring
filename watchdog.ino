#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <math.h>

// *********************************WiFi接続先の指定*******************************
// ############### 池田事務所 ###############
// String ssid = "Buffalo-G-0320";
// String pw = "cfn3sw44jxmkv";
// #########################################

// ############## 6製造ルーター #############
String ssid = "IKEDA-AP13";
String pw = "8286andAPad!m";
// #########################################

// *********************************機器番号の指定*********************************
String Device_Num = "0085"; // Device0058
// *******************************************************************************

// *********************************IPアドレスの指定*********************************
IPAddress ip(192, 168, 2, 6); // Device0058
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

#define JST 3600 * 9

/* ----------------------------------------------------
 * 初期化
---------------------------------------------------- */
void setup()
{
  Serial.begin(115200);

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

    String path = "/scripts/IoT/reception.php?";
    path = path + "number=" + Device_Num + "&color=" + String(status) + "&year=" + year + "&month=" + month + "&day=" + day + "&hh=" + hh + "&mm=" + mm + "&ss=" + ss;

    Serial.print("connecting to ");
    Serial.println(host);

    WiFiClient client;
    if (!client.connect(host, httpPort))
    {
      // TODO: ESP-NOW

      Serial.println("connection failed");
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

        // TODO: ESP-NOW

        return;
      }
    }

    lastDidSendMillis = millis();
  }

  delay(100);
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
