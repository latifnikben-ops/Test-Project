#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include "time.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// The OLED part 
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// wifi ssid and password
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// The NTP Client settings
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 0;

// The NTP Server settings
WiFiUDP udp;
const int NTP_PORT = 123;
const int NTP_PACKET_SIZE = 48;
byte ntpPacket[NTP_PACKET_SIZE];

// WEB SERVER 
WebServer server(80);

// STATUS FLAGS 
bool wifiOK = false;
bool ntpOK = false;

// OLED STATUS 
void showStatus(const char* l1, const char* l2 = "", const char* l3 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(l1);
  display.println(l2);
  display.println(l3);
  display.display();
}

// This is the main point of project, ANALOG CLOCK 
void drawAnalogClock(int hour, int minute, int second) {
  display.clearDisplay();
  int cx = 64, cy = 32, r = 30;
  display.drawCircle(cx, cy, r, SSD1306_WHITE);

  for (int i = 0; i < 12; i++) {
    float a = (i * 30 - 90) * PI / 180.0;
    display.drawLine(
      cx + cos(a) * (r - 2), cy + sin(a) * (r - 2),
      cx + cos(a) * r,       cy + sin(a) * r,
      SSD1306_WHITE
    );
  }

  auto hand = [&](float deg, int len) {
    float r = (deg - 90) * PI / 180.0;
    display.drawLine(cx, cy, cx + cos(r) * len, cy + sin(r) * len, SSD1306_WHITE);
  };

  hand((hour % 12) * 30 + minute * 0.5, 14);
  hand(minute * 6 + second * 0.1, 22);
  hand(second * 6, 28);

  display.display();
}

//  NTP SERVER HANDLER 
void handleNtpServer() {
  int packetSize = udp.parsePacket();
  if (!packetSize) return;

  udp.read(ntpPacket, NTP_PACKET_SIZE);
  memset(ntpPacket, 0, NTP_PACKET_SIZE);
  ntpPacket[0] = 0b00100100; // Server mode

  time_t now;
  time(&now);
  uint32_t ntpTime = now + 2208988800UL;

  ntpPacket[40] = (ntpTime >> 24) & 0xFF;
  ntpPacket[41] = (ntpTime >> 16) & 0xFF;
  ntpPacket[42] = (ntpTime >> 8) & 0xFF;
  ntpPacket[43] = ntpTime & 0xFF;

  udp.beginPacket(udp.remoteIP(), udp.remotePort());
  udp.write(ntpPacket, NTP_PACKET_SIZE);
  udp.endPacket();
}

// The  WEB PAGE part 
void handleRoot() {
  struct tm t;
  getLocalTime(&t);

  char timeStr[32];
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &t);

  String page =
    "<html><head><title>ESP32 WiFi Clock</title></head><body>"
    "<h2>ESP32 WiFi Analog Clock</h2>"
    "<p><b>WiFi:</b> Connected</p>"
    "<p><b>IP:</b> " + WiFi.localIP().toString() + "</p>"
    "<p><b>Current Time:</b> " + String(timeStr) + "</p>"
    "<p><b>NTP Client:</b> OK</p>"
    "<p><b>NTP Server:</b> Running (Port 123)</p>"
    "</body></html>";

  server.send(200, "text/html", page);
}

// SETUP 
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);

  showStatus("WiFi:", "CONNECTING...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  wifiOK = true;

  showStatus("WiFi OK", WiFi.localIP().toString().c_str());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm t;
  if (getLocalTime(&t)) ntpOK = true;

  udp.begin(NTP_PORT);

  server.on("/", handleRoot);
  server.begin();

  showStatus("System Ready", "Web UI ON", "NTP Server ON");
}

// LOOP 
void loop() {
  handleNtpServer();
  server.handleClient();

  struct tm t;
  if (getLocalTime(&t)) {
    drawAnalogClock(t.tm_hour, t.tm_min, t.tm_sec);
  }
  delay(1000);
}
