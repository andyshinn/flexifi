# Claude Code Memory - Flexifi Project

This file contains important project information and conventions for Claude Code to remember across sessions.

## Versioning and Release Management

### Version Tagging Convention
- **Use semantic versioning WITHOUT the 'v' prefix**: `1.0.1`, `1.2.0`, etc.
- **NOT**: `v1.0.1` (avoid the 'v' prefix)
- Example: `git tag 1.0.1` instead of `git tag v1.0.1`

### Release Process
1. Update version in `library.json`
2. Update `README.md` with any new features or changes
3. Create a new section in `RELEASE_NOTES.md` for the changes in the new version
4. Commit changes: `git add . && git commit -m "Release X.X.X"`
5. Create and push tag: `git tag X.X.X && git push origin main --tags`
6. Use `pio package pack` to create a new tar.gz file for the release
7. Use `pio pkg publish --no-interactive --type library` to publish to PlatformIO Registry
8. Use `gh release create X.X.X Flexifi-X.X.X.tar.gz --title "X.X.X" --notes "Release notes for this version only"` to create GitHub release

## Build and Development

### Asset Management
- Web assets (HTML/CSS/JS) are embedded at build time via `tools/embed_assets.py`
- Source files are in `src/web/` directory
- Generated embedded assets are in `src/generated/`
- To regenerate assets: `rm -rf src/generated/ && python3 tools/embed_assets.py`

### Test Project Structure
- `test_project/` contains local development environment
- Uses NeoPixel RGB LED for status indication
- NeoPixel power pin configuration: `NEOPIXEL_POWER_PIN = 2`
- ESP_LOG debugging enabled for comprehensive logging

### Build Commands
- **Build**: `pio run` (from test_project directory)
- **Upload**: `pio run -t upload`
- **Clean build**: `pio run -t clean`

### Configuration Macros
- **FLEXIFI_SCAN_THROTTLE_TIME**: Scan throttling time in milliseconds (default: 5000ms)
- **FLEXIFI_CONNECT_TIMEOUT**: WiFi connection timeout (default: 15000ms)
- **FLEXIFI_PORTAL_TIMEOUT**: Portal auto-shutdown timeout (default: 300000ms)
- **FLEXIFI_DEBUG_LEVEL**: Debug logging level 1-4 (default: 2)

## Hardware Configuration

### NeoPixel Status LED
- Data pin: `NEOPIXEL_PIN = 0`
- Power pin: `NEOPIXEL_POWER_PIN = 2` (must be pulled high)
- Count: `NEOPIXEL_COUNT = 1`
- Colors indicate WiFi and portal states

## Code Conventions

### Logging
- Use `ESP_LOG*` methods instead of `Serial.print*`
- Log levels: `ESP_LOGI`, `ESP_LOGD`, `ESP_LOGW`, `ESP_LOGE`
- Tag format: `static const char* TAG = "ComponentName";`

### Network Management
- WiFi profiles saved with `autoConnect = true` for automatic reconnection
- **Direct-connect auto-connect**: Attempts connection to saved profiles without scanning first
- Scan throttling: 30-second minimum between scans
- WebSocket + HTTP fallback for real-time updates
- Auto-connect retry delay bypassed for first connection attempt
- Scanning only triggered after all direct connections fail

## UI/UX Design Patterns

### Captive Portal Optimization
- Compact layout for macOS captive portal window constraints
- Inline button layout to save vertical space
- Side-by-side form fields (labels left, inputs right)
- Status color coding: yellow (scanning), red (errors), green (success)

### Network Display
- Hide "No networks found" during active scanning
- Show "Scanning..." with spinner during operations
- Disable scan button during active scans
- Manual connection form hidden behind "Enter Manually" toggle
- CSS signal strength indicators with color coding (red → orange → yellow → green)
- **Auto-select workflow**: Clicking network automatically fills SSID and focuses password field

### Signal Strength Visualization
- **CSS-based bars**: 5 bars with percentage heights (20%, 40%, 60%, 80%, 100%)
- **Granular 0-5 scale**: More detailed signal quality representation
- **Realistic RSSI mapping**: Tuned for real-world WiFi signal ranges
  - 5 bars: Green (≥ -30 dBm, exceptional signal)
  - 4 bars: Yellow-Green (≥ -50 dBm, excellent signal)
  - 3 bars: Yellow-Orange (≥ -60 dBm, good signal)
  - 2 bars: Red-Orange (≥ -70 dBm, fair signal)
  - 1 bar: Red (≥ -80 dBm, poor signal)
  - 0 bars: Dim red (< -80 dBm, very poor signal)
- **Color-bar consistency**: Colors always match the number of active bars
- **Enhanced animations**: 
  - Simplified scale-only animation to avoid color conflicts
  - Staggered timing for smooth visual introduction
  - Individual bar delays (0.05s, 0.1s, 0.15s, 0.2s, 0.25s)
- **Opacity system**: Active bars at full opacity, inactive bars at 20% opacity
- **Professional styling**: Rounded corners, proper spacing, transform origins
- **Debug logging**: Console output shows RSSI → Strength → Color mapping

## Known Issues and Debugging

### Network Display Problems
- If networks found on serial but not in web interface:
  1. Check browser console for JavaScript errors
  2. Verify WebSocket connection status
  3. Check `/networks.json` endpoint response
  4. Enable debug logging in `portal.js`

### Common Debug Steps
1. Check ESP_LOG output on serial console
2. Use browser developer tools console
3. Verify WebSocket connection in Network tab
4. Test `/networks.json` endpoint directly

## Development Environment

### Required Dependencies
- PlatformIO for ESP32 development
- Python 3 with `minify-html` package for asset optimization
- GitHub CLI (`gh`) for release management

### Key Files
- `library.json` - PlatformIO library configuration
- `RELEASE_NOTES.md` - Version changelog
- `tools/prepare_release.py` - Release preparation script
- `tools/embed_assets.py` - Asset embedding tool

## Architecture Notes

### Component Structure
- `Flexifi` - Main manager class
- `PortalWebServer` - Web server and WebSocket handling
- `StorageManager` - Dual storage (LittleFS + NVS)
- `TemplateManager` - Template system with variable injection
- `FlexifiParameter` - Custom parameter system

### Storage Strategy
- LittleFS for file-based storage with automatic fallback to NVS
- WiFi credentials stored as profiles with priority system
- Custom parameters persisted separately from WiFi settings

Last updated: 2025-07-07