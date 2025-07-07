# Flexifi

Modern async captive portal WiFi manager for ESP32 that combines the best features from existing libraries while solving their key limitations.

## Claude Assistant

Flexifi was mostly developed with the help of Claude, an AI assistant by Anthropic. It provided a majority of the code and suggestions throughout the development process. This statement is included to acknowledge the role of AI in enhancing the development experience. Check out [CLAUDE.md](CLAUDE.md) for more details on how Claude assisted in the development of Flexifi and [docs/PLAN.md](docs/PLAN.md) for the initial project plan.

## Features

- **AsyncWebServer Integration** - Uses your existing AsyncWebServer instance
- **Dual Communication Protocol** - WebSocket-driven real-time updates with GET/POST fallback
- **Flexible Storage System** - LittleFS primary with NVS fallback
- **Multiple Templates** - Modern, Classic, Minimal, and Custom template support
- **Event-Driven Architecture** - Rich callback system for all portal states
- **Non-blocking Operation** - Fully async design for responsive performance

## Quick Start

### Installation

Using PlatformIO:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    esp32async/ESPAsyncWebServer@^3.7.0
    bblanchon/ArduinoJson@^6.21.0
    Flexifi
    # Or for development: https://github.com/andyshinn/flexifi.git
```

**Note**: Web assets are automatically embedded during library installation via the postinstall script.

### Basic Usage

```cpp
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Flexifi.h>

AsyncWebServer server(80);
Flexifi portal(&server);

void setup() {
    Serial.begin(115200);

    // Configure callbacks
    portal.onWiFiConnect([](const String& ssid) {
        Serial.printf("Connected to: %s\n", ssid.c_str());
        portal.stopPortal();
    });

    // Try to load saved configuration
    if (!portal.loadConfig()) {
        // No saved config, start portal
        portal.startPortal("MyDevice-Setup");
    }

    server.begin();
}

void loop() {
    portal.loop();
}
```

## Templates

Flexifi includes three built-in responsive templates:

### Modern Template (Default)
- Clean Material Design-inspired interface
- Dark/light mode support
- Animated connection progress
- Mobile-first responsive layout

### Classic Template
- Traditional form-based layout
- Bootstrap-style components
- Broad browser compatibility

### Minimal Template
- Ultra-lightweight design (< 5KB)
- Essential features only
- Perfect for memory-constrained projects

### Custom Templates
```cpp
String customHTML = R"(
<!DOCTYPE html>
<html>
<head><title>{{TITLE}}</title></head>
<body>
    <h1>My Custom Portal</h1>
    <div id="networks">{{NETWORKS}}</div>
    <div id="status">{{STATUS}}</div>
    <script src="/portal.js"></script>
</body>
</html>
)";

portal.setCustomTemplate(customHTML);
```

## API Reference

### Core Methods

```cpp
// Portal Management
bool startPortal(const String& apName, const String& apPassword = "");
void stopPortal();
bool isPortalActive();

// Configuration
void setTemplate(const String& templateName);
void setCustomTemplate(const String& htmlTemplate);
bool saveConfig();
bool loadConfig();
void clearConfig();

// Network Management
void scanNetworks();
bool connectToWiFi(const String& ssid, const String& password);
String getNetworksJSON();
WiFiState getWiFiState();
```

### Event Callbacks

```cpp
// Portal events
portal.onPortalStart([]() { /* Portal started */ });
portal.onPortalStop([]() { /* Portal stopped */ });

// WiFi events
portal.onWiFiConnect([](const String& ssid) { /* Connected */ });
portal.onWiFiDisconnect([]() { /* Disconnected */ });
portal.onConnectStart([](const String& ssid) { /* Connecting... */ });
portal.onConnectFailed([](const String& ssid) { /* Failed */ });

// Scan events
portal.onScanComplete([](int count) { /* Scan done */ });

// Configuration events
portal.onConfigSave([](const String& ssid, const String& password) {
    /* Config saved */
});
```

## Configuration Options

### Compile-time Configuration

```cpp
// Storage configuration
#define FLEXIFI_FORCE_LITTLEFS    // Force LittleFS usage
#define FLEXIFI_FORCE_NVS         // Force NVS usage
#define FLEXIFI_DISABLE_LITTLEFS  // Disable LittleFS support
#define FLEXIFI_DISABLE_NVS       // Disable NVS support

// Feature configuration
#define FLEXIFI_DISABLE_WEBSOCKET // Disable WebSocket support

// Debug configuration
#define FLEXIFI_DEBUG_LEVEL 3     // 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug

// Network configuration
#define FLEXIFI_SCAN_TIMEOUT 10000    // WiFi scan timeout (ms)
#define FLEXIFI_CONNECT_TIMEOUT 15000 // Connection timeout (ms)
#define FLEXIFI_PORTAL_TIMEOUT 300000 // Portal timeout (ms)
```

### Dependency Issues

If you encounter dependency conflicts, specify exact packages:

```ini
[env:esp32dev]
lib_deps =
    esp32async/ESPAsyncWebServer@^3.7.0  # Specify owner to avoid conflicts
    bblanchon/ArduinoJson@^6.21.0        # Official ArduinoJson package
    Flexifi
```

### Runtime Configuration

```cpp
portal.setPortalTimeout(300000);   // 5 minutes
portal.setConnectTimeout(15000);   // 15 seconds
```

## WebSocket Communication

Flexifi uses WebSocket for real-time communication with automatic fallback to HTTP.

### Message Format

```json
{
    "type": "scan_complete|connect_start|connect_success|connect_failed|status_update",
    "data": {
        "networks": [...],
        "status": "scanning|connecting|connected|disconnected",
        "message": "Human readable status"
    }
}
```

### REST API Fallback

```
GET  /                  - Portal main page
GET  /scan              - Scan networks (JSON)
POST /connect           - Connect to network
GET  /status            - Connection status
POST /reset             - Reset configuration
GET  /networks.json     - Network list (JSON)
```

## Examples

- **[basic_usage](examples/basic_usage/)** - Simple portal setup
- **[advanced_callbacks](examples/advanced_callbacks/)** - Event handling and custom logic
- **[custom_template](examples/custom_template/)** - Custom UI templates
- **[multi_server](examples/multi_server/)** - Using with existing web server

## Storage System

Flexifi uses a dual storage system for maximum reliability:

### LittleFS (Primary)
- JSON-formatted credential storage
- Configuration file management
- Automatic partition management

### NVS (Fallback)
- Key-value credential storage
- Namespace isolation
- Flash wear leveling

### Automatic Failover
The library automatically handles storage initialization and failover between systems.

## Troubleshooting

For detailed troubleshooting information, see [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

### Quick Fixes

**Dependency conflicts:**
```ini
lib_deps =
    esp32async/ESPAsyncWebServer@^3.7.0  # Specify exact package
    bblanchon/ArduinoJson@^6.21.0
    Flexifi
```

**Assets not found:**
```bash
rm -rf src/generated/ && python3 tools/embed_assets.py
```

**Enable debug logging:**
```cpp
#define FLEXIFI_DEBUG_LEVEL 4
#include <Flexifi.h>
```

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Acknowledgments

Inspired by and improves upon:
- WiFiManager
- NetWizard
- EasyWifi

Built with modern C++ practices and async-first design principles.
