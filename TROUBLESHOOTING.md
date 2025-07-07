# Flexifi Troubleshooting Guide

## Common Issues and Solutions

### 📦 **Dependency Conflicts**

**Issue**: Multiple ESPAsyncWebServer packages found
```
Library Manager: Warning! More than one package has been found by ESPAsyncWebServer
```

**Solution**: Specify the exact package owner in your `platformio.ini`:
```ini
[env:esp32dev]
lib_deps =
    esp32async/ESPAsyncWebServer@^3.7.0  # Use this specific package
    bblanchon/ArduinoJson@^6.21.0        # Official ArduinoJson
    Flexifi
```

**Alternative packages** (if esp32async doesn't work):
- `me-no-dev/ESPAsyncWebServer@^1.2.3` (original, less maintained)
- `mathieucarbou/ESPAsyncWebServer@^3.6.0` (another active fork)

---

### 🔧 **Compilation Errors**

**Issue**: `web_assets.h not found`
```
fatal error: generated/web_assets.h: No such file or directory
```

**Solution**: Regenerate web assets:
```bash
# Delete generated assets
rm -rf src/generated/

# Regenerate (from library root)
python3 tools/embed_assets.py
```

**Alternative**: Use pre-compilation script in `platformio.ini`:
```ini
extra_scripts = pre:tools/embed_assets.py
```

---

### 📡 **Network Issues**

**Issue**: Portal doesn't start or can't connect to AP

**Troubleshooting steps**:
1. **Check memory**: ESP32 needs sufficient RAM for AP mode
2. **Verify initialization**: Call `portal.init()` before other operations
3. **Check power**: Insufficient power can cause WiFi issues
4. **Enable debugging**:
   ```cpp
   #define FLEXIFI_DEBUG_LEVEL 4
   #include <Flexifi.h>
   ```

---

### 💾 **Storage Issues**

**Issue**: Credentials not saving/loading

**Solutions**:
1. **Check storage initialization**:
   ```cpp
   if (!portal.init()) {
       Serial.println("Storage initialization failed");
   }
   ```

2. **Try different storage backend**:
   ```cpp
   #define FLEXIFI_FORCE_NVS  // Force NVS usage
   // or
   #define FLEXIFI_FORCE_LITTLEFS  // Force LittleFS usage
   ```

3. **Clear corrupted storage**:
   ```cpp
   portal.clearConfig();  // Clear all stored data
   ```

---

### 🌐 **Web Interface Issues**

**Issue**: Portal loads but interface doesn't work

**Troubleshooting**:
1. **Check browser compatibility**: Use modern browser with JavaScript enabled
2. **Try different device**: Test with phone/tablet
3. **Check WebSocket support**: Library falls back to HTTP automatically
4. **Verify assets**: Ensure web assets are embedded correctly

---

### 🔄 **Auto-Connect Issues**

**Issue**: Auto-connect not working with saved profiles

**Solutions**:
1. **Verify profiles exist**:
   ```cpp
   Serial.printf("Profile count: %d\n", portal.getWiFiProfileCount());
   ```

2. **Check auto-connect is enabled**:
   ```cpp
   portal.setAutoConnectEnabled(true);
   ```

3. **Set appropriate signal quality**:
   ```cpp
   portal.setMinSignalQuality(-75);  // Accept weaker signals
   ```

4. **Check profile priorities**:
   ```cpp
   String profiles = portal.getWiFiProfilesJSON();
   Serial.println(profiles);
   ```

---

### ⚠️ **Memory Issues**

**Issue**: ESP32 resets or crashes

**Solutions**:
1. **Increase partition size** in `platformio.ini`:
   ```ini
   board_build.partitions = huge_app.csv
   ```

2. **Reduce template size**:
   ```cpp
   portal.setTemplate("minimal");  // Use minimal template
   ```

3. **Disable features**:
   ```cpp
   #define FLEXIFI_DISABLE_WEBSOCKET  // Reduce memory usage
   ```

---

### 🐛 **Debug Information**

**Enable verbose logging**:
```cpp
#define FLEXIFI_DEBUG_LEVEL 4
#define CORE_DEBUG_LEVEL 4
#include <Flexifi.h>
```

**Check storage info**:
```cpp
String info = portal.getStorageInfo();
Serial.println(info);
```

**Monitor status**:
```cpp
void loop() {
    portal.loop();
    
    // Print status every 30 seconds
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 30000) {
        String status = portal.getStatusJSON();
        Serial.println(status);
        lastPrint = millis();
    }
}
```

---

## Getting Help

If you're still experiencing issues:

1. **Check examples**: Look at the [examples directory](examples/) for working code
2. **Enable debug logging**: Set `FLEXIFI_DEBUG_LEVEL 4` for detailed output
3. **Check hardware**: Verify ESP32 power supply and connections
4. **Report bugs**: Create an issue on [GitHub](https://github.com/andyshinn/flexifi/issues) with:
   - Full error message
   - Debug log output
   - Hardware details (board type, power supply)
   - Code snippet that reproduces the issue

## Known Limitations

- **Memory usage**: Templates require ~20KB RAM for larger templates
- **Concurrent connections**: Limited by ESP32 hardware (~4 simultaneous web clients)
- **Network scanning**: Limited to 2.4GHz networks only
- **Storage capacity**: NVS has 15-character key limit; LittleFS recommended for larger configs