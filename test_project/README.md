# Flexifi Local Test Project

This is a test project to verify the Flexifi library works correctly when loaded as a local dependency.

## Setup

1. Make sure you have PlatformIO installed
2. Open this directory in PlatformIO IDE or use the CLI
3. Build and upload to your ESP32

## Expected Behavior

1. **Auto-connect attempt**: The device will first try to connect to any saved WiFi networks
2. **Portal fallback**: If no saved networks are available, it starts a captive portal
3. **Status updates**: Every 30 seconds, it prints current status to serial monitor

## Portal Access

When the portal is active:
- **SSID**: `Flexifi Test`
- **Password**: `flexifi123`
- **URL**: `http://192.168.4.1`

## LED Indicators

- **Blinking**: Portal starting
- **Solid On**: WiFi connected
- **Fast Blinking**: Error state

## Serial Monitor

Set baud rate to **115200** to see debug output.

## Testing Features

This test project demonstrates:
- ✅ Library initialization
- ✅ WiFi profile management
- ✅ Auto-connect functionality
- ✅ Captive portal fallback
- ✅ Event callbacks
- ✅ Status monitoring
- ✅ Modern template

## Build Commands

```bash
# Build the project
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor

# Build, upload, and monitor in one command
pio run --target upload && pio device monitor
```

## Troubleshooting

If you get compilation errors:
1. Check that the path in `platformio.ini` points to your Flexifi directory
2. Ensure all dependencies are installed
3. Try cleaning the build: `pio run --target clean`

If the portal doesn't start:
1. Check serial output for error messages
2. Verify ESP32 has enough memory
3. Try a different ESP32 board
