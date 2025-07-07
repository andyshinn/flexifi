# AsyncWiFiPortal - Captive Portal WiFi Manager

## Project Overview

A modern, async-first captive portal WiFi manager for ESP32 that combines the best features from WifiManager, NetWizard, and EasyWifi while solving their key limitations.

## Core Features

### 1. **AsyncWebServer Integration**
- Uses existing AsyncWebServer instance passed to constructor
- No interference with user's existing web server setup
- Full async operation for responsive performance

### 2. **Dual Communication Protocol**
- WebSocket-driven real-time updates
- Automatic fallback to GET/POST for compatibility
- Progressive enhancement approach

### 3. **Flexible Storage System**
- Primary: LittleFS with automatic partition management
- Fallback: NVS (Non-Volatile Storage)
- Compile-time configuration macros

### 4. **Comprehensive Template System**
- Multiple built-in responsive templates
- Custom template support
- Template inheritance and customization

### 5. **Event-Driven Architecture**
- Rich callback system for all portal states
- Hooks for custom authentication, validation, and UI updates
- Non-blocking event handling

## Technical Architecture

### Library Structure
```
AsyncWiFiPortal/
├── src/
│   ├── AsyncWiFiPortal.h
│   ├── AsyncWiFiPortal.cpp
│   ├── PortalWebServer.h
│   ├── PortalWebServer.cpp
│   ├── StorageManager.h
│   ├── StorageManager.cpp
│   ├── TemplateManager.h
│   ├── TemplateManager.cpp
│   └── templates/
│       ├── modern.html
│       ├── classic.html
│       ├── minimal.html
│       └── custom.html
├── examples/
│   ├── basic_usage/
│   ├── advanced_callbacks/
│   ├── custom_template/
│   └── multi_server/
├── docs/
└── library.json
```

### Core Classes

#### 1. AsyncWiFiPortal (Main Class)
```cpp
class AsyncWiFiPortal {
public:
    // Constructor - takes existing AsyncWebServer
    AsyncWiFiPortal(AsyncWebServer* server);

    // Configuration
    void setTemplate(const String& templateName);
    void setCustomTemplate(const String& htmlTemplate);
    void setCredentials(const String& ssid, const String& password);

    // Portal Management
    bool startPortal(const String& apName, const String& apPassword = "");
    void stopPortal();
    bool isPortalActive();

    // Storage Management
    bool saveConfig();
    bool loadConfig();
    void clearConfig();

    // Event Callbacks
    void onPortalStart(std::function<void()> callback);
    void onPortalStop(std::function<void()> callback);
    void onWiFiConnect(std::function<void(const String&)> callback);
    void onWiFiDisconnect(std::function<void()> callback);
    void onConfigSave(std::function<void(const String&, const String&)> callback);
    void onScanComplete(std::function<void(int)> callback);

    // Network Management
    void scanNetworks();
    String getNetworksJSON();
    bool connectToWiFi(const String& ssid, const String& password);

private:
    AsyncWebServer* _server;
    PortalWebServer* _portalServer;
    StorageManager* _storage;
    TemplateManager* _templateManager;
    // ... other private members
};
```

#### 2. PortalWebServer (Web Interface Handler)
```cpp
class PortalWebServer {
public:
    PortalWebServer(AsyncWebServer* server, AsyncWiFiPortal* portal);

    void setupRoutes();
    void setupWebSocket();
    void broadcastStatus(const String& message);

private:
    void handleRoot(AsyncWebServerRequest* request);
    void handleScan(AsyncWebServerRequest* request);
    void handleConnect(AsyncWebServerRequest* request);
    void handleStatus(AsyncWebServerRequest* request);
    void handleReset(AsyncWebServerRequest* request);
    void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len);

    AsyncWebServer* _server;
    AsyncWebSocket* _ws;
    AsyncWiFiPortal* _portal;
};
```

#### 3. StorageManager (Persistent Storage)
```cpp
class StorageManager {
public:
    StorageManager();

    bool init();
    bool saveCredentials(const String& ssid, const String& password);
    bool loadCredentials(String& ssid, String& password);
    bool clearCredentials();

    bool saveConfig(const String& key, const String& value);
    String loadConfig(const String& key, const String& defaultValue = "");

private:
    bool _useLittleFS;
    bool _useNVS;

    bool initLittleFS();
    bool initNVS();

    // LittleFS methods
    bool saveLittleFS(const String& filename, const String& data);
    String loadLittleFS(const String& filename);

    // NVS methods
    bool saveNVS(const String& key, const String& value);
    String loadNVS(const String& key);
};
```

#### 4. TemplateManager (UI Templates)
```cpp
class TemplateManager {
public:
    TemplateManager();

    void setTemplate(const String& templateName);
    void setCustomTemplate(const String& htmlTemplate);

    String getPortalHTML();
    String processTemplate(const String& templateStr, const String& networks);

private:
    String _currentTemplate;
    String _customTemplate;

    String getBuiltinTemplate(const String& name);
    String replaceVariables(const String& html, const String& networks);
};
```

## Configuration Macros

### Compile-time Configuration
```cpp
// Storage configuration
#define AWIFIPORTAL_FORCE_LITTLEFS    // Force LittleFS usage
#define AWIFIPORTAL_FORCE_NVS         // Force NVS usage
#define AWIFIPORTAL_DISABLE_LITTLEFS  // Disable LittleFS support
#define AWIFIPORTAL_DISABLE_NVS       // Disable NVS support

// Feature configuration
#define AWIFIPORTAL_DISABLE_WEBSOCKET // Disable WebSocket support
#define AWIFIPORTAL_ENABLE_MDNS       // Enable mDNS support
#define AWIFIPORTAL_CUSTOM_CSS        // Enable custom CSS injection

// Debug configuration
#define AWIFIPORTAL_DEBUG_LEVEL 3     // 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug
#define AWIFIPORTAL_LOG_TAG "AsyncWiFiPortal"

// Network configuration
#define AWIFIPORTAL_SCAN_TIMEOUT 10000    // WiFi scan timeout (ms)
#define AWIFIPORTAL_CONNECT_TIMEOUT 15000 // Connection timeout (ms)
#define AWIFIPORTAL_PORTAL_TIMEOUT 300000 // Portal timeout (ms)
```

## Built-in Templates

### 1. Modern Template
- Clean, responsive design with CSS Grid
- Dark/light mode toggle
- Animated connection progress
- Real-time network scanning
- Mobile-first responsive layout

### 2. Classic Template
- Traditional form-based layout
- Bootstrap-style components
- Broad browser compatibility
- Minimal JavaScript requirements

### 3. Minimal Template
- Ultra-lightweight design
- Essential features only
- < 5KB total size
- Perfect for memory-constrained projects

### 4. Custom Template Support
- Template variable system: `{{NETWORKS}}`, `{{STATUS}}`, `{{TITLE}}`
- CSS injection points
- JavaScript event hooks
- Custom asset loading

## WebSocket Communication Protocol

### Message Format
```json
{
    "type": "scan_start|scan_complete|connect_start|connect_success|connect_failed|status_update",
    "data": {
        "networks": [...],
        "status": "connecting|connected|disconnected|scanning",
        "message": "Human readable status",
        "progress": 0-100
    }
}
```

### Fallback REST API
```
GET  /                  - Portal main page
GET  /scan              - Scan networks (JSON)
POST /connect           - Connect to network
GET  /status            - Connection status
POST /reset             - Reset configuration
GET  /networks.json     - Network list (JSON)
```

## Logging System

### ESP_LOG Integration
```cpp
// Automatic log level configuration
#if AWIFIPORTAL_DEBUG_LEVEL >= 4
    #define AWIFIPORTAL_LOGD(format, ...) ESP_LOGD(AWIFIPORTAL_LOG_TAG, format, ##__VA_ARGS__)
#else
    #define AWIFIPORTAL_LOGD(format, ...)
#endif

// Usage throughout library
AWIFIPORTAL_LOGI("Starting portal on AP: %s", apName.c_str());
AWIFIPORTAL_LOGD("WebSocket client connected: %u", client->id());
AWIFIPORTAL_LOGW("Failed to connect to %s, retrying...", ssid.c_str());
AWIFIPORTAL_LOGE("Storage initialization failed!");
```

## Usage Examples

### Basic Usage
```cpp
#include <AsyncWiFiPortal.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);
AsyncWiFiPortal portal(&server);

void setup() {
    Serial.begin(115200);

    // Configure portal
    portal.setTemplate("modern");
    portal.onWiFiConnect([](const String& ssid) {
        Serial.printf("Connected to: %s\n", ssid.c_str());
    });

    // Start portal if no saved credentials
    if (!portal.loadConfig()) {
        portal.startPortal("MyDevice-Setup");
    }

    server.begin();
}
```

### Advanced Callbacks
```cpp
portal.onPortalStart([]() {
    Serial.println("Portal started - configure via 192.168.4.1");
});

portal.onScanComplete([](int networks) {
    Serial.printf("Found %d networks\n", networks);
});

portal.onConfigSave([](const String& ssid, const String& password) {
    Serial.printf("Saved: %s\n", ssid.c_str());
    // Custom validation or additional storage
});
```

### Custom Template
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

## PlatformIO Integration

### library.json
```json
{
    "name": "AsyncWiFiPortal",
    "version": "1.0.0",
    "description": "Modern async captive portal WiFi manager for ESP32",
    "keywords": ["wifi", "captive portal", "esp32", "async"],
    "repository": {
        "type": "git",
        "url": "https://github.com/yourusername/AsyncWiFiPortal"
    },
    "authors": [{
        "name": "Your Name",
        "email": "your.email@example.com"
    }],
    "license": "MIT",
    "dependencies": {
        "ESPAsyncWebServer": "^1.2.3",
        "LittleFS": "^1.0.0"
    },
    "frameworks": ["arduino"],
    "platforms": ["espressif32"]
}
```

### platformio.ini Example
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    ESPAsyncWebServer
    AsyncWiFiPortal
build_flags =
    -DAWIFIPORTAL_DEBUG_LEVEL=3
    -DAWIFIPORTAL_ENABLE_MDNS
```

## Development Phases

### Phase 1: Core Infrastructure
- [ ] Basic class structure
- [ ] AsyncWebServer integration
- [ ] Storage manager (LittleFS + NVS)
- [ ] Simple template system
- [ ] Basic logging

### Phase 2: Portal Functionality
- [ ] WiFi scanning and connection
- [ ] Web interface routes
- [ ] Template processing
- [ ] Configuration persistence
- [ ] Basic callbacks

### Phase 3: Advanced Features
- [ ] WebSocket implementation
- [ ] Real-time updates
- [ ] Multiple templates
- [ ] Advanced callbacks
- [ ] Error handling

### Phase 4: Polish & Testing
- [ ] Comprehensive examples
- [ ] Documentation
- [ ] Unit tests
- [ ] Performance optimization
- [ ] Memory optimization

## Key Advantages

1. **No Platform Dependencies**: Works with standard ESP32 Arduino framework
2. **Server Integration**: Uses existing AsyncWebServer instance
3. **Modern UI**: WebSocket-driven with fallback support
4. **Flexible Storage**: Dual storage system with compile-time configuration
5. **Extensible**: Rich callback system for customization
6. **Responsive**: Async-first design for better performance
7. **Memory Efficient**: Configurable features to reduce memory usage
8. **PlatformIO Ready**: Drop-in library with proper dependency management

This design addresses all the pain points you mentioned while providing a modern, maintainable solution that's easy to integrate into existing projects.
