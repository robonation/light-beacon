#pragma once


#define DEFAULT_LED_PIN 17  // single data pin, top + side chained in series
#define DEFAULT_TOP_LED_COUNT 144   // 8x8 upward-facing panel
#define DEFAULT_SIDE_LED_COUNT 144  // outer strip; >64 and <100
#define DEFAULT_SIDE_FIRST false   // false = top is pixels [0, topCount)

// Wire color order, one of: "rgb", "rbg", "grb", "gbr", "brg", "bgr".
// WS2812/WS2812B are GRB; WS2815 varies by batch/vendor - if colors look
// swapped, change it from the web UI's Hardware section instead of here.
#define DEFAULT_COLOR_ORDER "grb"

#define LED_BRIGHTNESS_MAX 255

// Access point the ESP32 hosts. WPA2 requires 8+ characters; use "" for an
// open network instead.
#define AP_SSID "LightBeacon"
#define AP_PASSWORD "beacon123"

#define BLINK_HZ 0.5f  // full on/off cycles per second
