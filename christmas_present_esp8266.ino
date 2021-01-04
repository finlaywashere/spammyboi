#define ALL_PIN 3
#define BTN1 1
#define BTN2 2
#define BTN3 0

#define SECURITY_CODE ""
#define VERSION "1.01"

#define RESET_EEPROM 0
#define OVERRIDE 0
#define OVERRIDE_SSID ""
#define OVERRIDE_PSK ""

#define INDEX_PAGE "<html><body><form action=\"/setup_page\" method=\"post\"><label>SSID: </label><input type=\"text\" id=\"ssid\" name=\"ssid\"><br><label>PSK: </label><input type=\"password\" id=\"psk\" name=\"psk\"><br><input type=\"submit\" value=\"Activate\"></body></html>"

#include <Arduino.h>
#include <EEPROM.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

const char* setup_ssid = "Setup Network";
const char* setup_psk = "password";

const int addr_ssid = 1;
const int addr_psk = 33;

AsyncWebServer server(80);

int btn = 0;
int set = 0;
bool has_setup = false;

ESP8266WiFiMulti WiFiMulti;
boolean started = false;
long lastMillis = 0;

void setup() {
  EEPROM.begin(512);

  if (RESET_EEPROM == 1) {
    reset_esp();
    return;
  }

  if (EEPROM.read(0) != 1 && OVERRIDE != 1) {
    for (int i = 1; i < 512; i++)
      EEPROM.write(i, 0);
    EEPROM.commit();
    delay(200);


    WiFi.softAP(setup_ssid, setup_psk);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(200, "text/html", INDEX_PAGE);
    });
    server.on("/setup_page", HTTP_POST, [](AsyncWebServerRequest * request) {
      AsyncWebParameter* ssid = request->getParam("ssid", true);
      AsyncWebParameter* psk = request->getParam("psk", true);
      if (psk == nullptr || ssid == nullptr) {
        request->send(400, "text/html", "Invalid request!");
        return;
      }
      const char* ssid_str = ssid->value().c_str();
      const char* psk_str = psk->value().c_str();

      int ssid_len = strlen(ssid_str);
      int psk_len = strlen(psk_str);
      EEPROM.write(0, 1);
      for (int i = 0; i < 32; i++) {
        if (i < ssid_len)
          EEPROM.write(addr_ssid + i, ssid_str[i]);
      }
      for (int i = 0; i < 64; i++) {
        if (i < psk_len)
          EEPROM.write(addr_psk + i, psk_str[i]);
      }
      EEPROM.commit();
      request->send(200, "text/html", "Success!");
      delay(200);
      ESP.restart();
    });
    server.begin();
    while (1) {
      yield();
    }
  } else {
    init_program();
  }
}
void init_program() {
  WiFi.mode(WIFI_STA);
  char ssid[32];
  char psk[64];
  for (int i = 0; i < 32; i++) {
    ssid[i] = EEPROM.read(addr_ssid + i);
  }
  for (int i = 0; i < 64; i++) {
    psk[i] = EEPROM.read(addr_psk + i);
  }
  if (OVERRIDE == 0)
    WiFiMulti.addAP(ssid, psk);
  else
    WiFiMulti.addAP(OVERRIDE_SSID, OVERRIDE_PSK);
  pinMode(ALL_PIN, INPUT_PULLUP);
  pinMode(BTN1, OUTPUT);
  pinMode(BTN2, OUTPUT);
  pinMode(BTN3, OUTPUT);
  digitalWrite(BTN1, LOW);
  digitalWrite(BTN2, LOW);
  digitalWrite(BTN3, LOW);
}
void reset_esp() {
  for (int i = 0; i < 512; i++)
    EEPROM.write(i, 0);
  EEPROM.commit();
}

void loop() {
  wl_status_t status = WiFiMulti.run();
  if (status == WL_CONNECTED) {
    if (started == false) {
      WiFiClient client;
      HTTPClient http;
      if (http.begin(client, "http://server.finlaym.xyz/device/setup.php")) {
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        String ip = WiFi.localIP().toString();
        http.POST("cod=" SECURITY_CODE "&lan=" + ip + "&version=" VERSION);
      }
      started = true;
      http.end();
      client.flush();
      client.stop();
    }
    if (btn != 0 && (set == 2 || set == 1 || set == 4)) {
      WiFiClient client;
      HTTPClient http;
      if (http.begin(client, "http://server.finlaym.xyz/device/send.php")) {
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        http.POST("code=" SECURITY_CODE "&btn=" + String(btn));
      }
      http.end();
      client.flush();
      client.stop();
      delay(500);
    }
  } else if (status == 6) {
    // Wrong password
    EEPROM.write(0, 0);
    EEPROM.commit();
    ESP.restart();
  }
  if (digitalRead(ALL_PIN) == LOW) {
    digitalWrite(BTN1, LOW);
    digitalWrite(BTN2, HIGH);
    digitalWrite(BTN3, HIGH);
    if (digitalRead(ALL_PIN) == LOW) {
      btn = 1;
      set |= 1;
    }
    digitalWrite(BTN1, HIGH);
    digitalWrite(BTN2, LOW);
    digitalWrite(BTN3, HIGH);
    if (digitalRead(ALL_PIN) == LOW) {
      btn = 2;
      set |= 2;
    }
    digitalWrite(BTN1, HIGH);
    digitalWrite(BTN2, HIGH);
    digitalWrite(BTN3, LOW);
    if (digitalRead(ALL_PIN) == LOW) {
      btn = 3;
      set |= 4;
    }
    digitalWrite(BTN1, LOW);
    digitalWrite(BTN2, LOW);
    digitalWrite(BTN3, LOW);
  } else {
    btn = 0;
    set = 0;
  }
  if (set == 7) {
    if (lastMillis == 0) {
      lastMillis = millis();
      return;
    } else if (millis() - lastMillis >= 5000) {
      // This doesn't work ;-;
      // TODO: Fix this
      reset_esp();
      ESP.restart();
    }
  } else if (lastMillis != 0)
    lastMillis = 0;
}
