#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

#include "config.h"
#include "web_page.h"

namespace {

enum class Output : uint8_t { OFF, TOP, SIDE };
enum class Color : uint8_t { RED, GREEN, BLUE };

// Wire color order varies by chip/vendor/batch - WS2815 in particular is
// commonly RGB instead of WS2812's GRB, but it isn't consistent enough to
// assume, so it's exposed as a setting instead of hardcoded.
enum class ColorOrder : uint8_t { RGB, RBG, GRB, GBR, BRG, BGR };

const neoPixelType kColorOrderTypes[] = {
    NEO_RGB + NEO_KHZ800, NEO_RBG + NEO_KHZ800, NEO_GRB + NEO_KHZ800,
    NEO_GBR + NEO_KHZ800, NEO_BRG + NEO_KHZ800, NEO_BGR + NEO_KHZ800,
};

struct LightState {
  Output output = Output::OFF;
  Color color = Color::RED;
  uint8_t brightness = 128;
  bool blink = false;
};

// Top and side are two segments of one physically daisy-chained pixel
// buffer, not separate outputs. sideFirst picks which segment starts at
// pixel 0. All of this is user-configurable from the web UI and persisted
// in NVS, so it survives reboots and reflashes.
struct HardwareConfig {
  uint8_t pin = DEFAULT_LED_PIN;
  uint16_t topCount = DEFAULT_TOP_LED_COUNT;
  uint16_t sideCount = DEFAULT_SIDE_LED_COUNT;
  bool sideFirst = DEFAULT_SIDE_FIRST;
  ColorOrder colorOrder = ColorOrder::GRB;
};

constexpr uint8_t kMaxGpioPin = 48;
constexpr uint16_t kMinSegmentLeds = 1;
constexpr uint16_t kMaxSegmentLeds = 500;

LightState lightState;
HardwareConfig hwConfig;
bool blinkOnPhase = true;
unsigned long lastBlinkToggle = 0;
const unsigned long kBlinkHalfPeriodMs = static_cast<unsigned long>(500.0f / BLINK_HZ);

const IPAddress kApIP(192, 168, 4, 1);
const IPAddress kNetMask(255, 255, 255, 0);

DNSServer dnsServer;
WebServer server(80);
Preferences prefs;
Adafruit_NeoPixel pixels(DEFAULT_TOP_LED_COUNT + DEFAULT_SIDE_LED_COUNT,
                         DEFAULT_LED_PIN, NEO_GRB + NEO_KHZ800);

uint16_t topStartIndex() { return hwConfig.sideFirst ? hwConfig.sideCount : 0; }
uint16_t sideStartIndex() { return hwConfig.sideFirst ? 0 : hwConfig.topCount; }

uint32_t colorFor(Color c, uint8_t b) {
  switch (c) {
    case Color::RED:   return pixels.Color(b, 0, 0);
    case Color::GREEN: return pixels.Color(0, b, 0);
    case Color::BLUE:  return pixels.Color(0, 0, b);
  }
  return 0;
}

void applyOutput() {
  bool lit = !lightState.blink || blinkOnPhase;

  pixels.clear();
  if (lightState.output != Output::OFF && lit) {
    bool isTop = lightState.output == Output::TOP;
    uint16_t start = isTop ? topStartIndex() : sideStartIndex();
    uint16_t count = isTop ? hwConfig.topCount : hwConfig.sideCount;
    pixels.fill(colorFor(lightState.color, lightState.brightness), start, count);
  }
  pixels.show();
}

const char *outputToStr(Output o) {
  switch (o) {
    case Output::TOP:  return "top";
    case Output::SIDE: return "side";
    default: return "off";
  }
}

const char *colorToStr(Color c) {
  switch (c) {
    case Color::GREEN: return "green";
    case Color::BLUE:  return "blue";
    default: return "red";
  }
}

bool outputFromStr(const String &s, Output &out) {
  if (s == "top")  { out = Output::TOP;  return true; }
  if (s == "side") { out = Output::SIDE; return true; }
  if (s == "off")  { out = Output::OFF;  return true; }
  return false;
}

bool colorFromStr(const String &s, Color &out) {
  if (s == "red")   { out = Color::RED;   return true; }
  if (s == "green") { out = Color::GREEN; return true; }
  if (s == "blue")  { out = Color::BLUE;  return true; }
  return false;
}

const char *colorOrderToStr(ColorOrder o) {
  switch (o) {
    case ColorOrder::RGB: return "rgb";
    case ColorOrder::RBG: return "rbg";
    case ColorOrder::GBR: return "gbr";
    case ColorOrder::BRG: return "brg";
    case ColorOrder::BGR: return "bgr";
    default: return "grb";
  }
}

bool colorOrderFromStr(const String &s, ColorOrder &out) {
  if (s == "rgb") { out = ColorOrder::RGB; return true; }
  if (s == "rbg") { out = ColorOrder::RBG; return true; }
  if (s == "grb") { out = ColorOrder::GRB; return true; }
  if (s == "gbr") { out = ColorOrder::GBR; return true; }
  if (s == "brg") { out = ColorOrder::BRG; return true; }
  if (s == "bgr") { out = ColorOrder::BGR; return true; }
  return false;
}

void loadHardwareConfig() {
  prefs.begin("lbeacon", true);
  hwConfig.pin = prefs.getUChar("pin", DEFAULT_LED_PIN);
  hwConfig.topCount = prefs.getUShort("topCount", DEFAULT_TOP_LED_COUNT);
  hwConfig.sideCount = prefs.getUShort("sideCount", DEFAULT_SIDE_LED_COUNT);
  hwConfig.sideFirst = prefs.getBool("sideFirst", DEFAULT_SIDE_FIRST);
  ColorOrder co;
  hwConfig.colorOrder =
      colorOrderFromStr(prefs.getString("colorOrder", DEFAULT_COLOR_ORDER), co)
          ? co
          : ColorOrder::GRB;
  prefs.end();
}

void saveHardwareConfig() {
  prefs.begin("lbeacon", false);
  prefs.putUChar("pin", hwConfig.pin);
  prefs.putUShort("topCount", hwConfig.topCount);
  prefs.putUShort("sideCount", hwConfig.sideCount);
  prefs.putBool("sideFirst", hwConfig.sideFirst);
  prefs.putString("colorOrder", colorOrderToStr(hwConfig.colorOrder));
  prefs.end();
}

// Reconfigures the physical chain to match hwConfig. Pixel content is
// cleared as a side effect (old buffer contents don't carry over sensibly
// across a length/pin/order change), so the caller should follow up with
// applyOutput() to redraw the current light state.
void applyHardwareConfig() {
  pixels.updateLength(hwConfig.topCount + hwConfig.sideCount);
  pixels.updateType(kColorOrderTypes[static_cast<uint8_t>(hwConfig.colorOrder)]);
  pixels.setPin(hwConfig.pin);
  pixels.begin();
}

void sendStateJson() {
  JsonDocument doc;
  doc["output"] = outputToStr(lightState.output);
  doc["color"] = colorToStr(lightState.color);
  doc["brightness"] = lightState.brightness;
  doc["blink"] = lightState.blink;
  doc["tempC"] = temperatureRead();
  String body;
  serializeJson(doc, body);
  server.send(200, "application/json", body);
}

void sendConfigJson() {
  JsonDocument doc;
  doc["pin"] = hwConfig.pin;
  doc["topCount"] = hwConfig.topCount;
  doc["sideCount"] = hwConfig.sideCount;
  doc["sideFirst"] = hwConfig.sideFirst;
  doc["colorOrder"] = colorOrderToStr(hwConfig.colorOrder);
  String body;
  serializeJson(doc, body);
  server.send(200, "application/json", body);
}

void handleRoot() {
  server.send(200, "text/html", WEB_PAGE_HTML);
}

void handleGetState() {
  sendStateJson();
}

void handleSetState() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"missing body\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"bad json\"}");
    return;
  }

  if (doc["output"].is<const char *>()) {
    Output o;
    if (!outputFromStr(doc["output"].as<String>(), o)) {
      server.send(400, "application/json", "{\"error\":\"bad output\"}");
      return;
    }
    lightState.output = o;
  }

  if (doc["color"].is<const char *>()) {
    Color c;
    if (!colorFromStr(doc["color"].as<String>(), c)) {
      server.send(400, "application/json", "{\"error\":\"bad color\"}");
      return;
    }
    lightState.color = c;
  }

  if (doc["brightness"].is<int>()) {
    int b = doc["brightness"].as<int>();
    lightState.brightness = (uint8_t)constrain(b, 0, LED_BRIGHTNESS_MAX);
  }

  if (doc["blink"].is<bool>()) {
    lightState.blink = doc["blink"].as<bool>();
    blinkOnPhase = true;
    lastBlinkToggle = millis();
  }

  applyOutput();
  sendStateJson();
}

void handleGetConfig() {
  sendConfigJson();
}

void handleSetConfig() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"missing body\"}");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"bad json\"}");
    return;
  }

  if (doc["pin"].is<int>()) {
    int p = doc["pin"].as<int>();
    if (p < 0 || p > kMaxGpioPin) {
      server.send(400, "application/json", "{\"error\":\"pin out of range\"}");
      return;
    }
    hwConfig.pin = (uint8_t)p;
  }

  if (doc["topCount"].is<int>()) {
    int n = doc["topCount"].as<int>();
    if (n < kMinSegmentLeds || n > kMaxSegmentLeds) {
      server.send(400, "application/json", "{\"error\":\"topCount out of range\"}");
      return;
    }
    hwConfig.topCount = (uint16_t)n;
  }

  if (doc["sideCount"].is<int>()) {
    int n = doc["sideCount"].as<int>();
    if (n < kMinSegmentLeds || n > kMaxSegmentLeds) {
      server.send(400, "application/json", "{\"error\":\"sideCount out of range\"}");
      return;
    }
    hwConfig.sideCount = (uint16_t)n;
  }

  if (doc["sideFirst"].is<bool>()) {
    hwConfig.sideFirst = doc["sideFirst"].as<bool>();
  }

  if (doc["colorOrder"].is<const char *>()) {
    ColorOrder co;
    if (!colorOrderFromStr(doc["colorOrder"].as<String>(), co)) {
      server.send(400, "application/json", "{\"error\":\"bad colorOrder\"}");
      return;
    }
    hwConfig.colorOrder = co;
  }

  saveHardwareConfig();
  applyHardwareConfig();
  applyOutput();
  sendConfigJson();
}

void handleUpdateUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    // Go dark for the duration of the flash: the loop is blocked reading
    // the upload anyway, and this leaves the rig in a clean state if the
    // update fails partway through.
    lightState.output = Output::OFF;
    lightState.blink = false;
    pixels.clear();
    pixels.show();

    Serial.printf("OTA start: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("OTA success: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
  }
}

void handleUpdateEnd() {
  bool ok = !Update.hasError();
  server.sendHeader("Connection", "close");
  server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "Update failed, still running old firmware");
  if (ok) {
    delay(500);
    ESP.restart();
  }
}

void handleCaptivePortalRedirect() {
  // Sends any unrecognized/probe request back to the control page so phones
  // and laptops that check for a captive portal open it automatically.
  server.sendHeader("Location", String("http://") + kApIP.toString() + "/", true);
  server.send(302, "text/plain", "");
}

}  // namespace

void setup() {
  Serial.begin(115200);

  loadHardwareConfig();
  applyHardwareConfig();
  pixels.show();

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(kApIP, kApIP, kNetMask);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  dnsServer.start(53, "*", kApIP);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleGetState);
  server.on("/api/set", HTTP_POST, handleSetState);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handleSetConfig);
  server.on("/update", HTTP_POST, handleUpdateEnd, handleUpdateUpload);

  // Common OS captive-portal probe paths.
  server.on("/generate_204", HTTP_GET, handleCaptivePortalRedirect);
  server.on("/gen_204", HTTP_GET, handleCaptivePortalRedirect);
  server.on("/hotspot-detect.html", HTTP_GET, handleCaptivePortalRedirect);
  server.on("/library/test/success.html", HTTP_GET, handleCaptivePortalRedirect);
  server.on("/ncsi.txt", HTTP_GET, handleCaptivePortalRedirect);
  server.on("/connecttest.txt", HTTP_GET, handleCaptivePortalRedirect);

  server.onNotFound(handleCaptivePortalRedirect);
  server.begin();

  applyOutput();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  if (lightState.blink) {
    unsigned long now = millis();
    if (now - lastBlinkToggle >= kBlinkHalfPeriodMs) {
      lastBlinkToggle = now;
      blinkOnPhase = !blinkOnPhase;
      applyOutput();
    }
  }
}
