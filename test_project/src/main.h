/*
 * Flexifi Local Test Project - Header File
 *
 * This header contains all the declarations, constants, and includes
 * for the Flexifi test project with NeoPixel status indication.
 */

#ifndef MAIN_H
#define MAIN_H

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Flexifi.h>
#include <Adafruit_NeoPixel.h>

// Hardware Configuration
const int NEOPIXEL_PIN = 0;              // RGB LED pin (adjust for your board)
const int NEOPIXEL_POWER_PIN = 2;        // NeoPixel I2C power pin (set to your GPIO pin)
const int NEOPIXEL_COUNT = 1;             // Number of NeoPixels

// External objects (defined in main.cpp)
extern AsyncWebServer server;
extern Flexifi portal;
extern Adafruit_NeoPixel pixel;

/*
 * NeoPixel Status Indication System
 * =================================
 *
 * The onboard RGB LED provides visual feedback for different system states:
 *
 * SOLID COLORS:
 *   • OFF (Black)     - System idle/disconnected
 *   • GREEN           - Successfully connected to WiFi
 *   • ORANGE          - Captive portal is active (waiting for user configuration)
 *
 * BLINKING COLORS:
 *   • BLUE            - Scanning for WiFi networks
 *   • YELLOW          - Attempting to connect to WiFi
 *   • MAGENTA         - Captive portal starting up
 *   • RED             - Connection failed/error state
 *
 * Note: Pin configuration varies by board:
 *       - ESP32-S3: Pin 48 for data, some boards need separate power pin
 *       - ESP32-C3: Pin 8 for data
 *       - ESP32-S2: Pin 38 for data
 *       - Set NEOPIXEL_POWER_PIN to your board's I2C power pin if needed
 */

// NeoPixel Status Colors
extern const uint32_t COLOR_OFF;              // Black - off
extern const uint32_t COLOR_SCANNING;         // Blue - scanning for networks
extern const uint32_t COLOR_CONNECTING;       // Yellow - connecting to WiFi
extern const uint32_t COLOR_CONNECTED;        // Green - connected to WiFi
extern const uint32_t COLOR_PORTAL_STARTING;  // Magenta - portal starting
extern const uint32_t COLOR_PORTAL_ACTIVE;    // Orange - portal active
extern const uint32_t COLOR_ERROR;            // Red - error state

// Function Declarations

// Core Functions
void setup();
void loop();

// Status and Utility Functions
void printStatus();
String wifiStateToString(WiFiState state);
String portalStateToString(PortalState state);

// NeoPixel Functions
void setNeoPixelColor(uint32_t color, bool immediate = true);
void updateNeoPixelStatus();

// Legacy Functions (for compatibility)
void printConnectionStatus();
void printPortalStatus();
void checkProfileStatus();
void updateStatusLED();
void handleStateMachine();
void handleCheckingProfilesState(unsigned long stateTime);
void handleAutoConnectingState(unsigned long stateTime);
void handleWiFiConnectedState(unsigned long stateTime);
void handlePortalStartingState(unsigned long stateTime);
void handlePortalActiveState(unsigned long stateTime);
void handleErrorState(unsigned long stateTime);
void setupEventCallbacks();
void printProfileStatus();

#endif // MAIN_H
