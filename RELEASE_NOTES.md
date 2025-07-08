# Flexifi Release Notes

## v1.0.2 - 2025-07-08

### 🔒 New Security Feature
- **Automatic Password Generation**: Added optional secure password generation for captive portal
  - Constructor parameter: `Flexifi portal(&server, true)` enables auto-generation
  - Uses 8-character alphanumeric passwords with cryptographically secure seeding
  - Automatic usage when no password provided to `startPortal()`
  - Periodic logging every 30 seconds (configurable via `FLEXIFI_PASSWORD_LOG_INTERVAL`)

### 🖥️ Developer Experience
- **LCD Display Support**: Added `getGeneratedPassword()` method for external display integration
- **Release Build Logging**: Enhanced logging configuration for release environments
  - Added `CORE_DEBUG_LEVEL=3` support for ESP_LOGI messages in release builds
  - Configurable password logging interval via macro

### 📖 Documentation
- **Comprehensive README Updates**: Added complete password generation documentation
- **API Reference**: Updated with new `getGeneratedPassword()` method
- **Configuration Guide**: Added `FLEXIFI_PASSWORD_LOG_INTERVAL` macro documentation
- **Usage Examples**: Added practical examples for password generation and LCD display

### 🔧 Technical Improvements
- **Smart Password Logic**: Generated passwords only used when no password provided
- **Configurable Timing**: `FLEXIFI_PASSWORD_LOG_INTERVAL` macro for customizable logging frequency
- **Memory Efficient**: Minimal overhead when password generation is disabled

---

## v1.0.1 - 2025-07-07

### 🎨 UI/UX Improvements
- **Compact Interface Design**: Reduced spacing and optimized layout for captive portal environments
- **Inline Button Layout**: Moved Scan, Enter Manually, and Reset buttons inline with "Available Networks" header
- **Side-by-side Form Fields**: Labels now appear to the left of inputs for more compact forms
- **Responsive Container**: Increased container width to 600px for better text fitting
- **Smart Content Toggling**: Network list automatically hides when manual entry form is shown

### 📱 Enhanced Captive Portal Experience
- **Header Optimization**: Title and subtitle now displayed side-by-side to save vertical space
- **macOS Compatibility**: Improved layout to fit within macOS captive portal window constraints
- **Status Color Coding**:
  - Yellow background for scanning/throttled states
  - Red background for error conditions
  - Proper status preservation during operations

### 🔧 Scan System Overhaul
- **Throttling Management**: Proper 30-second scan throttling with countdown timer display
- **WebSocket + HTTP Parity**: Both connection methods now handle throttling identically
- **Improved Feedback**: "Scanning..." message during active scans instead of "No networks found"
- **Status Protection**: Important status messages no longer get overridden by network updates
- **Better Error Handling**: Clear visual feedback for scan failures and network issues

### 🌐 WiFi Connection Reliability
- **Auto-Connect Fix**: Resolved issue where devices would disconnect and fail to auto-reconnect
- **WiFi Profile Storage**: Now properly saves WiFi profiles with auto-connect enabled instead of basic credentials
- **Mode Management**: Fixed WiFi mode handling to stay in STA mode after portal stops
- **Network Cache Management**: Proper invalidation of network cache when switching modes

### 🎯 Resource Optimization
- **Portal Cleanup**: ~99% of portal resources are freed when connected and portal stopped
- **Minimal Footprint**: Only essential WiFi monitoring remains active when connected
- **Memory Management**: Proper cleanup of WebSocket, DNS, and web server resources

### 🛠️ Developer Experience
- **Enhanced Debugging**: Added comprehensive logging for scan operations and auto-connect behavior
- **Asset Minification**: Integrated minify-html for 26.7% reduction in web asset size (3.6 KB savings)
- **Live Preview Support**: Maintained CSS/JS external references for development testing

### 🐛 Bug Fixes
- **JSON Parsing**: Fixed malformed JSON responses causing "string did not match expected pattern" errors
- **Network Display**: Resolved issue where networks were found on serial but not displayed in UI
- **Status Synchronization**: Fixed WebSocket and HTTP status message consistency
- **Form Validation**: Improved error handling for network connection attempts

### 📋 Technical Details
- Proper WiFi profile creation with `autoConnect = true` and priority system
- Enhanced scan throttling with server-side timing calculations
- Improved WebSocket message handling for immediate scan responses
- Better status message hierarchy to prevent important notifications from being overridden

---

## v1.0.0 - 2025-07-06

## 🚀 New Features

### High Priority Features Completed:
- ✅ Custom parameter injection system for user-defined fields
- ✅ Network quality filtering and signal strength display
- ✅ Build-time asset embedding (HTML/CSS/JS separation)

### Core Capabilities:
- AsyncWebServer integration with existing server instances
- Dual storage system (LittleFS + NVS) with automatic failover
- Multiple responsive templates (Modern, Classic, Minimal)
- WebSocket communication with HTTP fallback
- Real-time network scanning and connection status
- Custom parameter validation and persistence
- State machine examples for robust connection management

## 🎨 Templates

- **Modern Template**: Material Design-inspired responsive UI
- **Classic Template**: Traditional Bootstrap-style interface
- **Minimal Template**: Ultra-lightweight < 3KB template
- **Custom Template Support**: Full variable injection system

## 🔧 Developer Experience

- Separated web assets for better development workflow
- Automatic asset embedding via PlatformIO
- Comprehensive examples and documentation
- State-driven architecture examples
- Advanced callback system

## 📦 Installation

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    ESPAsyncWebServer
    ArduinoJson
    Flexifi
```

## 🛠️ Breaking Changes

- Migrated from inline templates to embedded assets
- Updated template variable system
- Enhanced parameter API

## 🐛 Bug Fixes

- Improved memory management for parameters
- Better error handling in storage systems
- Enhanced WebSocket reliability

## 📋 Known Issues

- ESP32 Arduino IDE include path warnings (expected)
- Legacy CSS/JS methods deprecated (use embedded assets)

## 🔗 Links

- [GitHub Repository](https://github.com/andyshinn/flexifi)
- [Documentation](README.md)
- [Examples](examples/)

Generated on: 2025-07-06 13:47:16
