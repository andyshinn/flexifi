# Flexifi Web Assets

This directory contains the web assets (HTML, CSS, JavaScript) for the Flexifi captive portal interface. These files are automatically embedded into the C++ binary during compilation.

## 📁 Directory Structure

```
src/web/
├── templates/         # HTML templates
│   ├── modern.html    # Modern responsive template
│   ├── classic.html   # Traditional form-based template
│   └── minimal.html   # Lightweight minimal template
├── css/              # Stylesheets
│   ├── modern.css    # Modern template styles
│   ├── classic.css   # Classic template styles
│   └── minimal.css   # Minimal template styles
└── js/               # JavaScript files
    └── portal.js     # Portal functionality and WebSocket handling
```

## 🔧 Development Workflow

### 1. Edit Assets
Edit the HTML, CSS, and JavaScript files directly in this directory using your preferred editor with full syntax highlighting and formatting.

### 2. Embed Assets
Run the embedding script to convert assets to C++ headers:

```bash
python3 tools/embed_assets.py
```

This generates:
- `src/generated/web_assets.h` - Header with embedded assets
- `src/generated/web_assets.cpp` - Implementation with lookup functions

### 3. Build Project
The embedded assets are automatically included when you compile the Flexifi library.

## 🎨 Template Variables

All templates support these variables:

### Core Variables
- `{{TITLE}}` - Page title (default: "Flexifi Setup")
- `{{STATUS}}` - Current status message with styling
- `{{NETWORKS}}` - List of available WiFi networks
- `{{CUSTOM_PARAMETERS}}` - User-defined form fields

### Asset Variables (Auto-injected)
- `{{CSS_MODERN}}` - Modern template CSS
- `{{CSS_CLASSIC}}` - Classic template CSS  
- `{{CSS_MINIMAL}}` - Minimal template CSS
- `{{JS_PORTAL}}` - Portal JavaScript functionality

### Example Template Usage
```html
<!DOCTYPE html>
<html>
<head>
    <title>{{TITLE}}</title>
    <style>{{CSS_MODERN}}</style>
</head>
<body>
    <h1>{{TITLE}}</h1>
    <div id="status">{{STATUS}}</div>
    <div id="networks">{{NETWORKS}}</div>
    
    <form id="connectForm">
        <input type="text" id="ssid" placeholder="Network Name">
        <input type="password" id="password" placeholder="Password">
        {{CUSTOM_PARAMETERS}}
        <button type="submit">Connect</button>
    </form>
    
    <script>{{JS_PORTAL}}</script>
</body>
</html>
```

## 🚀 PlatformIO Integration

### Automatic Asset Embedding (Recommended)

When you install Flexifi via PlatformIO, assets are automatically embedded during installation:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = 
    ESPAsyncWebServer
    ArduinoJson
    Flexifi  # Assets embedded automatically via postinstall script
```

### Manual Asset Embedding (Development)

For local development or custom builds, add this to your `platformio.ini`:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = 
    ESPAsyncWebServer
    ArduinoJson
    file:///path/to/flexifi
extra_scripts = 
    pre:tools/embed_assets.py
```

### Force Asset Regeneration

To regenerate assets (if you've modified templates):

```bash
# Delete existing assets
rm -rf src/generated/

# Regenerate assets
python3 tools/embed_assets.py
```

## 🎨 Customization

### Creating Custom Templates
1. Create new HTML file in `templates/`
2. Add corresponding CSS file in `css/` (optional)
3. Run embedding script
4. Use template: `portal.setTemplate("your_template_name")`

### Template Guidelines
- Use semantic HTML5 elements
- Include required form elements: `#ssid`, `#password`, `#connectForm`
- Include required containers: `#status`, `#networks`
- Add `{{CUSTOM_PARAMETERS}}` placeholder for user-defined fields
- Ensure mobile responsiveness
- Test with different screen sizes

### JavaScript Integration
The portal.js file provides:
- WebSocket communication with automatic fallback
- Network scanning and selection
- Form submission handling
- Status updates and animations
- Custom parameter support

### Key JavaScript Functions
- `scanNetworks()` - Trigger network scan
- `selectNetwork(ssid)` - Auto-fill SSID from network list
- `connectToWiFi()` - Submit connection form
- `resetConfig()` - Reset configuration
- `updateStatus(status, message)` - Update status display
- `updateNetworks(networks)` - Update network list

## 🛠️ Advanced Features

### Asset Compression
The embedding script supports:
- Raw string literals for clean C++ code
- Automatic escaping of special characters
- Size optimization for memory-constrained devices

### Development Mode
You can modify the TemplateManager to load from LittleFS during development:

```cpp
#ifdef FLEXIFI_DEV_MODE
    // Load from LittleFS for hot-reload during development
    return loadTemplateFromFS(templateName);
#else
    // Use embedded assets for production
    return FlexifiAssets::getTemplate(templateName);
#endif
```

## 📝 Best Practices

1. **Keep it Lightweight**: Templates are embedded in flash memory
2. **Mobile First**: Design for mobile devices primarily
3. **Progressive Enhancement**: Ensure functionality without JavaScript
4. **Accessibility**: Use proper labels and ARIA attributes
5. **Security**: Avoid inline scripts, use CSP-friendly patterns
6. **Testing**: Test on actual ESP32 hardware with limited memory

## 🔍 Debugging

### Asset Loading Issues
1. Check generated files exist in `src/generated/`
2. Verify embedding script ran without errors
3. Ensure TemplateManager includes the generated header
4. Check serial output for template loading messages

### Template Rendering Issues
1. Verify all template variables are properly replaced
2. Check CSS selectors match HTML structure
3. Test JavaScript functionality in browser first
4. Monitor WebSocket connection status

## 📄 File Size Optimization

Current asset sizes (approximate):
- Modern template: ~8KB (HTML + CSS + JS)
- Classic template: ~6KB (HTML + CSS + JS)  
- Minimal template: ~3KB (HTML + CSS + JS)

Tips for optimization:
- Minimize CSS by removing unused styles
- Compress JavaScript by removing comments
- Use efficient CSS selectors
- Optimize images (if any) before embedding

This asset system provides the perfect balance of development convenience and runtime efficiency for embedded devices!