# Flexifi NeoPixel Status Example

This example demonstrates how to use an onboard RGB LED (NeoPixel) to provide visual feedback for the Flexifi WiFi captive portal system.

## Hardware Requirements

- ESP32 board with onboard RGB LED (NeoPixel)
- Common boards with onboard RGB LEDs:
  - ESP32-S3 DevKit (pin 48)
  - ESP32-C3 DevKit (pin 8)
  - ESP32-S2 DevKit (pin 38)

## Pin Configuration

The example defaults to pin 48 (common on ESP32-S3 boards). If your board uses a different pin, update the `NEOPIXEL_PIN` constant in `main.cpp`:

```cpp
const int NEOPIXEL_PIN = 48;  // Change to your board's RGB LED pin
```

## Status Indication

The RGB LED provides the following visual feedback:

### Solid Colors
- **🔴 OFF (Black)** - System idle/disconnected
- **🟢 GREEN** - Successfully connected to WiFi
- **🟠 ORANGE** - Captive portal is active (waiting for user configuration)

### Blinking Colors
- **🔵 BLUE** - Scanning for WiFi networks
- **🟡 YELLOW** - Attempting to connect to WiFi
- **🟣 MAGENTA** - Captive portal starting up
- **🔴 RED** - Connection failed/error state

## Features

- **Automatic Status Detection**: Continuously monitors WiFi and portal states
- **Event-Driven Updates**: Responds immediately to Flexifi callbacks
- **Smart Blinking**: Different states use appropriate visual patterns
- **Low Brightness**: Set to 50% brightness to avoid being too bright
- **Efficient Updates**: Only updates every 500ms to reduce processing overhead

## Usage

1. Connect your ESP32 board with onboard RGB LED
2. Adjust `NEOPIXEL_PIN` if necessary
3. Upload the firmware
4. Observe the LED status as the system:
   - Scans for networks (blinking blue)
   - Attempts auto-connect (blinking yellow)
   - Starts captive portal (solid orange)
   - Connects to WiFi (solid green)

## Integration with Your Project

To add NeoPixel status indication to your own Flexifi project:

1. Add the Adafruit NeoPixel library dependency:
   ```ini
   lib_deps = 
       adafruit/Adafruit NeoPixel@^1.12.0
   ```

2. Include the header and initialize:
   ```cpp
   #include <Adafruit_NeoPixel.h>
   
   const int NEOPIXEL_PIN = 48;  // Adjust for your board
   Adafruit_NeoPixel pixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
   ```

3. Set up Flexifi event callbacks with color updates:
   ```cpp
   portal.onPortalStart([]() {
       setNeoPixelColor(COLOR_PORTAL_ACTIVE);
   });
   
   portal.onWiFiConnect([](const String& ssid) {
       setNeoPixelColor(COLOR_CONNECTED);
   });
   ```

4. Add the status update function to your main loop:
   ```cpp
   void loop() {
       portal.loop();
       updateNeoPixelStatus();  // Call every 500ms
   }
   ```

## Benefits

- **Instant Visual Feedback**: Users can see system status at a glance
- **Troubleshooting Aid**: Different colors help identify connection issues
- **Professional Appearance**: Smooth status transitions and appropriate colors
- **Minimal Code**: Easy to integrate into existing projects
- **Flexible**: Colors and behaviors can be easily customized

## Customization

You can customize the colors and behaviors by modifying the color constants and the `updateNeoPixelStatus()` function. The example provides a solid foundation that can be extended for specific use cases.