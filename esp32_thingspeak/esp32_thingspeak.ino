/*
 * ============================================================================
 * ESP32 WiFi Co-Processor  -  ThingSpeak stage
 * ============================================================================
 * Role: receive commands + sensor frames from the STM32 over UART and publish
 * to ThingSpeak over WiFi. The ESP32 does no sensing and makes no decisions -
 * the STM32 orchestrates the server round-robin and tells the ESP32 what to do.
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
 *   #SERVER,MQTT         - make the MQTT broker active (inactive this stage)
 *   #SERVER,NONE         - close/idle the active server (the "rest" phase)
 *   #DATA,<p1>,<p2>,<p3>,<temp_x10>   - publish one sensor sample
 *   #WIFI,<ssid>,<password>           - store new WiFi credentials in flash
 * temp_x10 is temperature in tenths of a degree C (234 = 23.4 C).
 *
 * Credentials: stored in NVS flash via the Preferences library, so they
 * survive reboots. The hardcoded values below are only a first-boot fallback;
 * once a #WIFI command is received, the stored credentials take over.
 *
 * --------------------------------------------------------------------------
 * BEFORE FLASHING:
 *  1. Install the CH340 USB-serial driver on Windows 10.
 *  2. Arduino IDE: add the ESP32 board package, select "ESP32 Dev Module".
 *  3. Put your ThingSpeak Write API Key in THINGSPEAK_API_KEY below.
 *     WiFi credentials can be left as-is and set later over Bluetooth.
 *  4. If upload fails at "Connecting...", hold the BOOT button until it starts.
 * ============================================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>

/* ----- USER CONFIGURATION ----- */
const char* THINGSPEAK_API_KEY  = "EVZKERX2SZKLXD13";
const char* THINGSPEAK_HOST     = "http://api.thingspeak.com/update";

/* First-boot fallback WiFi credentials (used only if none are stored yet).
 * Normally you set these over Bluetooth with: WIFI:ssid;password */
const char* DEFAULT_WIFI_SSID   = "TOPNET_E9FB";
const char* DEFAULT_WIFI_PASS   = "7TYYNSPSZ4G9";

/* ----- UART2 link to the STM32 ----- */
#define STM32_RX_PIN 16
#define STM32_TX_PIN 17
#define STM32_BAUD   115200
HardwareSerial STM32Serial(2);

/* ----- Persistent storage for WiFi credentials ----- */
Preferences prefs;
String wifiSsid;
String wifiPass;

/* ----- Server selection (driven entirely by the STM32) ----- */
enum ServerType { SRV_NONE, SRV_THINGSPEAK, SRV_MQTT };
ServerType activeServer = SRV_NONE;

/* ----- Line-assembly buffer for incoming UART data ----- */
static char   lineBuf[160];   /* large enough for #WIFI with a 63-char pass */
static size_t lineLen = 0;

/* ----------------------------------------------------------------------------
 * loadCreds - read stored WiFi credentials, falling back to the defaults.
 * --------------------------------------------------------------------------*/
void loadCreds() {
  prefs.begin("wifi", true);                 /* read-only */
  wifiSsid = prefs.getString("ssid", DEFAULT_WIFI_SSID);
  wifiPass = prefs.getString("pass", DEFAULT_WIFI_PASS);
  prefs.end();
  Serial.print("WiFi credentials in use, SSID = ");
  Serial.println(wifiSsid);
}

/* ----------------------------------------------------------------------------
 * saveCreds - persist new WiFi credentials to flash.
 * --------------------------------------------------------------------------*/
void saveCreds(const String& ssid, const String& pass) {
  prefs.begin("wifi", false);                /* read-write */
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
 * disconnectWiFi - drop the WiFi link (used when the STM32 sends #SERVER,NONE).
 * --------------------------------------------------------------------------*/
void disconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect(true);
    Serial.println("WiFi: disconnected (rest phase)");
  }
}

/* ----------------------------------------------------------------------------
 * publishThingSpeak - send one sample to ThingSpeak via an HTTP GET.
 * Fields: 1=pot1, 2=pot2, 3=pot3, 4=temperature (in C).
 * --------------------------------------------------------------------------*/
void publishThingSpeak(long p1, long p2, long p3, long temp_x10) {
  if (!connectWiFi()) {
    Serial.println("ThingSpeak: skipped, no WiFi");
    return;
  }

  /* temp_x10 is tenths of a degree; format as a normal decimal. */
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
      connectWiFi();                 /* bring WiFi up for the slot */
    } else if (strcmp(arg, "MQTT") == 0) {
      activeServer = SRV_MQTT;
      Serial.println("Server: MQTT active (inactive in this stage)");
    } else if (strcmp(arg, "NONE") == 0) {
      activeServer = SRV_NONE;
      Serial.println("Server: NONE (idle/rest)");
      disconnectWiFi();              /* close the connection during rest */
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
    String ssid = String(p);
    String pass = String(comma + 1);
    saveCreds(ssid, pass);
    /* Apply immediately: drop any current link so the next connect uses
     * the new credentials. */
    disconnectWiFi();
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
      Serial.println("MQTT active: #DATA ignored for now (next stage)");
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
  Serial.println("ESP32 WiFi co-processor - ThingSpeak stage");

  STM32Serial.begin(STM32_BAUD, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);

  loadCreds();
  /* Do not auto-connect here; wait for the STM32 to open a server slot. */
}

/* ----------------------------------------------------------------------------
 * loop - assemble UART lines and dispatch them
 * --------------------------------------------------------------------------*/
void loop() {
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
