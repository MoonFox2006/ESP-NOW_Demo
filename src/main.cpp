#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "EspNowHelper.h"

//#define USE_BROADCAST

const uint8_t LED_PIN = 2;
const bool LED_LEVEL = LOW;

static const char WIFI_SSID[] PROGMEM = "******";
static const char WIFI_PSWD[] PROGMEM = "******";

class EspNowServerPlus : public EspNowServer {
protected:
  void onReceive(const uint8_t *mac, const uint8_t *data, uint8_t len);

  uint32_t received;
  uint32_t lastTime;
  uint8_t mac[6];

  friend void setup();
};

class EspNowClientPlus : public EspNowClient {
public:
  EspNowClientPlus(uint8_t channel, const uint8_t *mac) : EspNowClient(channel, mac) {}

protected:
  void onReceive(const uint8_t *mac, const uint8_t *data, uint8_t len);
};

EspNowGeneric *esp_now;
ESP8266WebServer *http;

static String macToString(const uint8_t mac[]);

void EspNowServerPlus::onReceive(const uint8_t *mac, const uint8_t *data, uint8_t len) {
  digitalWrite(LED_PIN, LED_LEVEL);
  ++received;
  lastTime = millis();
  memcpy(this->mac, mac, sizeof(this->mac));
  Serial.print(F("ESP-NOW packet received from "));
  Serial.println(macToString(mac));
#ifndef USE_BROADCAST
  if (! findPeer(mac)) {
    Serial.print(F("Add new peer "));
    if (addPeer(mac))
      Serial.println(F("successful"));
    else
      Serial.println(F("fail!"));
  }
#endif
  digitalWrite(LED_PIN, ! LED_LEVEL);
}

void EspNowClientPlus::onReceive(const uint8_t *mac, const uint8_t *data, uint8_t len) {
  digitalWrite(LED_PIN, LED_LEVEL);
  Serial.print(F("ESP-NOW packet received from "));
  Serial.println(macToString(mac));
  digitalWrite(LED_PIN, ! LED_LEVEL);
}

static void halt(const __FlashStringHelper *msg) {
  Serial.println(msg);
  Serial.println(F("System halted!"));
  Serial.flush();

  ESP.deepSleep(0);
}

static void restart(const __FlashStringHelper *msg) {
  Serial.println(msg);
  Serial.println(F("System restarted!"));
  Serial.flush();

  ESP.restart();
}

static String macToString(const uint8_t mac[]) {
  char str[18];

  sprintf_P(str, PSTR("%02X:%02X:%02X:%02X:%02X:%02X"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  return String(str);
}

void setup() {
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
  Serial.println();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, ! LED_LEVEL);

  Serial.print(F("SDK version: "));
  Serial.println(ESP.getSdkVersion());

  {
    int8_t channel;
    uint8_t mac[6];
    uint32_t start;

    Serial.print(F("Waiting for ESP-NOW server"));
    channel = espNowFindServer(mac);
    if (! channel) {
      const uint32_t WAIT_SERVER_TIMEOUT = 15000; // 15 sec.

      start = millis();
      while ((! channel) && (millis() - start <= WAIT_SERVER_TIMEOUT)) {
        for (uint8_t i = 0; i < 4; ++i) { // Wait 1 sec.
          digitalWrite(LED_PIN, LED_LEVEL);
          delay(25);
          digitalWrite(LED_PIN, ! LED_LEVEL);
          delay(225);
        }
        Serial.print('.');
        channel = espNowFindServer(mac);
      }
    }
    if (channel) {
      Serial.print(F(" OK (on channel "));
      Serial.print(channel);
      Serial.print(F(", mac: "));
      Serial.print(macToString(mac));
      Serial.println(')');
      esp_now = new EspNowClientPlus(channel, mac);
      if (esp_now->begin()) {
        Serial.println(F("ESP-NOW client started"));
      } else {
        restart(F("ESP-NOW client init fail!"));
      }
    } else {
      const uint32_t WIFI_TIMEOUT = 60000; // 60 sec.

      char ssid[sizeof(WIFI_SSID)];
      char pswd[sizeof(WIFI_PSWD)];

      Serial.println(F(" fail!"));
      strcpy_P(ssid, WIFI_SSID);
      strcpy_P(pswd, WIFI_PSWD);
      WiFi.begin(ssid, pswd);
      Serial.print(F("Connecting to SSID \""));
      Serial.print(ssid);
      Serial.print('"');
      start = millis();
      while ((! WiFi.isConnected()) && (millis() - start <= WIFI_TIMEOUT)) {
        digitalWrite(LED_PIN, LED_LEVEL);
        delay(25);
        digitalWrite(LED_PIN, ! LED_LEVEL);
        delay(475);
        Serial.print('.');
      }
      if (WiFi.isConnected()) {
        Serial.print(F(" OK (IP: "));
        Serial.print(WiFi.localIP());
        Serial.println(')');
        esp_now = new EspNowServerPlus();
        if (esp_now->begin()) {
          http = new ESP8266WebServer(80);
          if (http) {
            http->onNotFound([]() {
              http->send_P(404, PSTR("text/plain"), PSTR("Page not found!"));
            });
            http->on(F("/"), []() {
              String page = F("<!DOCTYPE html>\n"
                "<html>\n"
                "<head>\n"
                "<title>ESP-NOW Demo</title>\n"
                "<meta http-equiv=\"refresh\" content=\"2;URL=/\">\n"
                "</head>\n"
                "<body>\n");

              if (! ((EspNowServerPlus*)esp_now)->received) {
                page += F("No ESP-NOW packet received yet");
              } else {
                page += ((EspNowServerPlus*)esp_now)->received;
                page += F(" ESP-NOW packet(s) received\n"
                  "Last packet received from ");
                page += macToString(((EspNowServerPlus*)esp_now)->mac);
                page += ' ';
                page += ((millis() - ((EspNowServerPlus*)esp_now)->lastTime) / 1000);
                page += F(" sec. ago");
              }
              page += F("\n"
                "</body>\n"
                "</html>");
              http->send(200, F("text/html"), page);
            });
            http->begin();
          }
          Serial.println(F("ESP-NOW server started"));
        } else {
          restart(F("ESP-NOW server init fail!"));
        }
      } else {
        restart(F(" FAIL!"));
      }
    }
  }
}

void loop() {
  const uint32_t SEND_PERIOD = 5000; // 5 sec.

  static uint32_t lastSend = 0;

  if ((! lastSend) || (millis() - lastSend >= SEND_PERIOD)) {
#ifdef USE_BROADCAST
    Serial.print(F("Sending broardcast message "));
    if (esp_now->sendBroadcast((uint8_t*)&lastSend, sizeof(lastSend)))
#else
    Serial.print(F("Sending unicast message "));
    if (esp_now->peerCount() && esp_now->sendAll((uint8_t*)&lastSend, sizeof(lastSend)))
#endif
      Serial.println(F("OK"));
    else
      Serial.println(F("FAIL!"));
    lastSend = millis();
  }

  if (http)
    http->handleClient();
}
