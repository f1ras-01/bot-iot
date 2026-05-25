/*
 * ============================================================================
 * ESP32 WiFi Co-Processor  -  ThingSpeak + Mosquitto (MQTT)
 * ============================================================================
 * Role: receive commands + sensor frames from the STM32 over UART and publish
 * them to whichever server the STM32 has selected. The ESP32 does no sensing
 * and makes no decisions - the STM32 orchestrates the server round-robin.
 *
 * Link to STM32:
 *   UART2 on the ESP32 (GPIO16 = RX2, GPIO17 = TX2), 115200 baud, 8N1.
 *   ESP32 GPIO16 (RX2) <- STM32 PB6 (USART1_TX)
 *   ESP32 GPIO17 (TX2) -> STM32 PB7 (USART1_RX)
 *   Common ground between the two boards is mandatory.
 *   UART0 is left for USB flashing / Serial Monitor - do NOT use it here.
 *
 * Protocol from STM32 (one '\n'-terminated ASCII line per message):
 *   #SERVER,THINGSPEAK   - make ThingSpeak the active server
 *   #SERVER,MQTT         - make the Mosquitto broker the active server
 *   #SERVER,NONE         - close/idle the active server (the "rest" phase)
 *   #DATA,<p1>,<p2>,<p3>,<temp_x10>   - publish one sensor sample
 *   #WIFI,<ssid>,<password>           - store new WiFi credentials in flash
 * temp_x10 is temperature in tenths of a degree C (234 = 23.4 C).
 *
 * Only one server is active at a time, enforced by the STM32's round-robin.
 *
 * MQTT publish (one value per topic, plain numeric payloads):
 *   robot/sensors/pot1  ->  2048
 *   robot/sensors/pot2  ->  1530
 *   robot/sensors/pot3  ->  900
 *   robot/sensors/temp  ->  23.4
 *
 * --------------------------------------------------------------------------
 * BEFORE FLASHING:
 *  1. Install the CH340 USB-serial driver on Windows 10.
 *  2. Arduino IDE: add the ESP32 board package, select "ESP32 Dev Module".
 *  3. Library Manager: install "PubSubClient" by Nick O'Leary.
 *  4. Fill in THINGSPEAK_API_KEY and MQTT_BROKER_IP below.
 *     WiFi credentials can be left as-is and set later over Bluetooth.
 *  5. If upload fails at "Connecting...", hold the BOOT button until it starts.
 * ============================================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <PubSubClient.h>

/* ----- USER CONFIGURATION ----- */
const char* THINGSPEAK_API_KEY = "YOUR_THINGSPEAK_WRITE_API_KEY";
const char* THINGSPEAK_HOST    = "http://api.thingspeak.com/update";

/* Mosquitto broker: the LAN IP of the PC running Mosquitto (see `ipconfig`).
 * Port 1883 is the MQTT default. */
const char* MQTT_BROKER_IP     = "192.168.1.20";
const int   MQTT_BROKER_PORT   = 1883;
const char* MQTT_CLIENT_ID     = "esp32-robot";

/* Each sensor value is published on its own sub-topic, e.g.
 * robot/sensors/pot1, robot/sensors/pot2, robot/sensors/pot3,
 * robot/sensors/temp. */
const char* MQTT_TOPIC_BASE    = "robot/sensors";

/* First-boot fallback WiFi credentials (used only if none are stored yet).
 * Normally you set these over Bluetooth with: WIFI:ssid;password */
const char* DEFAULT_WIFI_SSID  = "YOUR_WIFI_SSID";
const char* DEFAULT_WIFI_PASS  = "YOUR_WIFI_PASSWORD";

/* ----- UART2 link to the STM32 ----- */
#define STM32_RX_PIN 16
#define STM32_TX_PIN 17
#define STM32_BAUD   115200
HardwareSerial STM32Serial(2);

/* ----- Persistent storage for WiFi credentials ----- */
Preferences prefs;
String wifiSsid;
String wifiPass;

/* ----- Network clients ----- */
WiFiClient   wifiClient;             /* shared TCP socket for MQTT */
PubSubClient mqtt(wifiClient);

/* ----- Server selection (driven entirely by the STM32) ----- */
enum ServerType { SRV_NONE, SRV_THINGSPEAK, SRV_MQTT };
ServerType activeServer = SRV_NONE;

/* ----- Line-assembly buffer for incoming UART data ----- */
static char   lineBuf[160];
static size_t lineLen = 0;

/* ----------------------------------------------------------------------------
 * loadCreds / saveCreds - WiFi credentials in NVS flash.
 * --------------------------------------------------------------------------*/
void loadCreds() {
  prefs.begin("wifi", true);
  wifiSsid = prefs.getString("ssid", DEFAULT_WIFI_SSID);
  wifiPass = prefs.getString("pass", DEFAULT_WIFI_PASS);
  prefs.end();
  Serial.print("WiFi credentials in use, SSID = ");
  Serial.println(wifiSsid);
}

void saveCreds(const String& ssid, const String& pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  wifiSsid = ssid;
  wifiPass = pass;
  Serial.print("WiFi credentials stored, SSID = ");
  Serial.println(ssid);
}

/* ----------------------------------------------------------------------------
 * connectWiFi - (re)connect to the access point, with a bounded wait.
 * --------------------------------------------------------------------------*/
bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.print("WiFi: connecting to ");
  Serial.println(wifiSsid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi: connected, IP = ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("WiFi: connection FAILED");
  return false;
}

/* ----------------------------------------------------------------------------
 * disconnectServers - close MQTT and WiFi (the STM32 "rest" phase).
 * --------------------------------------------------------------------------*/
void disconnectServers() {
  if (mqtt.connected()) {
    mqtt.disconnect();
    Serial.println("MQTT: disconnected");
  }
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect(true);
    Serial.println("WiFi: disconnected (rest phase)");
  }
}

/* ----------------------------------------------------------------------------
 * connectMQTT - ensure the MQTT client is connected to the broker.
 * --------------------------------------------------------------------------*/
bool connectMQTT() {
  if (!connectWiFi()) return false;
  if (mqtt.connected()) return true;

  mqtt.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT);

  Serial.print("MQTT: connecting to ");
  Serial.print(MQTT_BROKER_IP);
  Serial.print(':');
  Serial.println(MQTT_BROKER_PORT);

  /* Anonymous connect (broker has allow_anonymous true). If you later add a
   * password file, use: mqtt.connect(MQTT_CLIENT_ID, "user", "pass") */
  unsigned long start = millis();
  while (!mqtt.connect(MQTT_CLIENT_ID) && millis() - start < 8000) {
    Serial.print('.');
    delay(500);
  }
  Serial.println();

  if (mqtt.connected()) {
    Serial.println("MQTT: connected");
    return true;
  }
  /* mqtt.state() codes: -4 timeout, -2 connect failed (broker unreachable /
   * firewall), 5 not authorized. */
  Serial.print("MQTT: connect FAILED, state = ");
  Serial.println(mqtt.state());
  return false;
}

/* ----------------------------------------------------------------------------
 * publishThingSpeak - send one sample to ThingSpeak via an HTTP GET.
 * --------------------------------------------------------------------------*/
void publishThingSpeak(long p1, long p2, long p3, long temp_x10) {
  if (!connectWiFi()) {
    Serial.println("ThingSpeak: skipped, no WiFi");
    return;
  }

  char tempStr[12];
  long whole = temp_x10 / 10;
  long frac  = temp_x10 % 10;
  if (frac < 0) frac = -frac;
  snprintf(tempStr, sizeof(tempStr), "%ld.%ld", whole, frac);

  String url = String(THINGSPEAK_HOST) +
               "?api_key=" + THINGSPEAK_API_KEY +
               "&field1="  + String(p1) +
               "&field2="  + String(p2) +
               "&field3="  + String(p3) +
               "&field4="  + String(tempStr);

  HTTPClient http;
  http.begin(url);
  int code = http.GET();

  if (code > 0) {
    String resp = http.getString();
    Serial.print("ThingSpeak: HTTP ");
    Serial.print(code);
    Serial.print(", entry id = ");
    Serial.println(resp);
    if (resp == "0") {
      Serial.println("ThingSpeak: update rejected (rate limit - 1 per 15 s)");
    }
  } else {
    Serial.print("ThingSpeak: request failed, error ");
    Serial.println(http.errorToString(code));
  }
  http.end();
}

/* ----------------------------------------------------------------------------
 * publishMQTT - send each sample value to its own Mosquitto topic.
 *   robot/sensors/pot1, /pot2, /pot3, /temp
 * --------------------------------------------------------------------------*/
void publishMQTT(long p1, long p2, long p3, long temp_x10) {
  if (!connectMQTT()) {
    Serial.println("MQTT: skipped, not connected");
    return;
  }

  /* Temperature as a decimal string (temp_x10 is tenths of a degree). */
  char tempStr[12];
  long whole = temp_x10 / 10;
  long frac  = temp_x10 % 10;
  if (frac < 0) frac = -frac;
  const char* sign = (temp_x10 < 0 && whole == 0) ? "-" : "";
  snprintf(tempStr, sizeof(tempStr), "%s%ld.%ld", sign, whole, frac);

  /* Per-value topic strings and their payloads. */
  char topic[40];
  char value[16];
  bool ok = true;

  snprintf(topic, sizeof(topic), "%s/pot1", MQTT_TOPIC_BASE);
  snprintf(value, sizeof(value), "%ld", p1);
  ok &= mqtt.publish(topic, value);

  snprintf(topic, sizeof(topic), "%s/pot2", MQTT_TOPIC_BASE);
  snprintf(value, sizeof(value), "%ld", p2);
  ok &= mqtt.publish(topic, value);

  snprintf(topic, sizeof(topic), "%s/pot3", MQTT_TOPIC_BASE);
  snprintf(value, sizeof(value), "%ld", p3);
  ok &= mqtt.publish(topic, value);

  snprintf(topic, sizeof(topic), "%s/temp", MQTT_TOPIC_BASE);
  ok &= mqtt.publish(topic, tempStr);

  if (ok) {
    Serial.printf("MQTT: published pot1=%ld pot2=%ld pot3=%ld temp=%s\n",
                  p1, p2, p3, tempStr);
  } else {
    Serial.println("MQTT: one or more publishes FAILED");
  }
}

/* ----------------------------------------------------------------------------
 * handleLine - parse one complete '\n'-terminated line from the STM32.
 * --------------------------------------------------------------------------*/
void handleLine(char* line) {
  if (line[0] != '#') return;

  /* ---- #SERVER,<name> ---- */
  if (strncmp(line, "#SERVER,", 8) == 0) {
    const char* arg = line + 8;
    if (strcmp(arg, "THINGSPEAK") == 0) {
      activeServer = SRV_THINGSPEAK;
      Serial.println("Server: THINGSPEAK active");
      /* ThingSpeak is HTTP-only; just make sure WiFi is up. */
      if (mqtt.connected()) mqtt.disconnect();
      connectWiFi();
    } else if (strcmp(arg, "MQTT") == 0) {
      activeServer = SRV_MQTT;
      Serial.println("Server: MQTT active");
      connectMQTT();
    } else if (strcmp(arg, "NONE") == 0) {
      activeServer = SRV_NONE;
      Serial.println("Server: NONE (idle/rest)");
      disconnectServers();
    } else {
      Serial.print("Unknown server: ");
      Serial.println(arg);
    }
    return;
  }

  /* ---- #WIFI,<ssid>,<password> ---- */
  if (strncmp(line, "#WIFI,", 6) == 0) {
    char* p = line + 6;
    char* comma = strchr(p, ',');
    if (comma == NULL) {
      Serial.println("Malformed #WIFI line");
      return;
    }
    *comma = '\0';
    saveCreds(String(p), String(comma + 1));
    disconnectServers();   /* force a reconnect with the new credentials */
    return;
  }

  /* ---- #DATA,<p1>,<p2>,<p3>,<temp_x10> ---- */
  if (strncmp(line, "#DATA,", 6) == 0) {
    long p1, p2, p3, tx10;
    int n = sscanf(line + 6, "%ld,%ld,%ld,%ld", &p1, &p2, &p3, &tx10);
    if (n != 4) {
      Serial.print("Malformed #DATA line: ");
      Serial.println(line);
      return;
    }

    Serial.printf("RX #DATA  p1=%ld p2=%ld p3=%ld temp_x10=%ld\n",
                  p1, p2, p3, tx10);

    if (activeServer == SRV_THINGSPEAK) {
      publishThingSpeak(p1, p2, p3, tx10);
    } else if (activeServer == SRV_MQTT) {
      publishMQTT(p1, p2, p3, tx10);
    } else {
      Serial.println("No server active: #DATA ignored");
    }
    return;
  }

  Serial.print("Unknown message: ");
  Serial.println(line);
}

/* ----------------------------------------------------------------------------
 * setup
 * --------------------------------------------------------------------------*/
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("ESP32 WiFi co-processor - ThingSpeak + MQTT");

  STM32Serial.begin(STM32_BAUD, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);

  loadCreds();
  /* Do not auto-connect here; wait for the STM32 to open a server slot. */
}

/* ----------------------------------------------------------------------------
 * loop - assemble UART lines, dispatch them, and service the MQTT client.
 * --------------------------------------------------------------------------*/
void loop() {
  /* PubSubClient needs loop() called regularly to keep the connection
   * alive and process keepalive pings. */
  if (activeServer == SRV_MQTT && mqtt.connected()) {
    mqtt.loop();
  }

  while (STM32Serial.available()) {
    char c = (char)STM32Serial.read();

    if (c == '\n' || c == '\r') {
      if (lineLen > 0) {
        lineBuf[lineLen] = '\0';
        handleLine(lineBuf);
        lineLen = 0;
      }
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    } else {
      lineLen = 0;   /* overlong line: discard, resync on next newline */
    }
  }
}
