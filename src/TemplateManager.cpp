#include "TemplateManager.h"
#include "Flexifi.h"
#include "generated/web_assets.h"
#include <ArduinoJson.h>

TemplateManager::TemplateManager() :
    _currentTemplate("modern"),
    _customTemplate(""),
    _usingCustomTemplate(false) {
}

TemplateManager::~TemplateManager() {
}

void TemplateManager::setTemplate(const String& templateName) {
    FLEXIFI_LOGD("Setting template to: %s", templateName.c_str());

    if (isValidTemplate(templateName)) {
        _currentTemplate = templateName;
        _usingCustomTemplate = false;
        FLEXIFI_LOGI("Template set to: %s", templateName.c_str());
    } else {
        FLEXIFI_LOGW("Invalid template name: %s, using default", templateName.c_str());
        _currentTemplate = "modern";
        _usingCustomTemplate = false;
    }
}

void TemplateManager::setCustomTemplate(const String& htmlTemplate) {
    FLEXIFI_LOGD("Setting custom template (%d chars)", htmlTemplate.length());

    if (htmlTemplate.isEmpty()) {
        FLEXIFI_LOGW("Custom template is empty, reverting to default");
        _usingCustomTemplate = false;
        return;
    }

    _customTemplate = _sanitizeTemplate(htmlTemplate);
    _usingCustomTemplate = true;
    FLEXIFI_LOGI("Custom template set successfully");
}

String TemplateManager::getCurrentTemplate() const {
    return _usingCustomTemplate ? "custom" : _currentTemplate;
}

String TemplateManager::getPortalHTML(const String& customParameters) const {
    FLEXIFI_LOGD("Generating portal HTML");

    String html;

    if (_usingCustomTemplate) {
        html = _customTemplate;
    } else {
        html = _getBuiltinTemplate(_currentTemplate);
    }

    // Basic variable replacement for static content
    html = replaceVariables(html, "[]", "ready", "Flexifi Setup", customParameters);

    return html;
}

String TemplateManager::processTemplate(const String& templateStr, const String& networks,
                                       const String& customParameters) const {
    FLEXIFI_LOGD("Processing template with %d networks", networks.length());

    String processedHTML = templateStr;

    // Replace template variables
    processedHTML = replaceVariables(processedHTML, networks, "ready", "Flexifi Setup", customParameters);

    return processedHTML;
}

bool TemplateManager::isValidTemplate(const String& templateName) const {
    return (templateName == "modern" ||
            templateName == "classic" ||
            templateName == "minimal" ||
            templateName == "default");
}

String TemplateManager::getAvailableTemplates() const {
    return "modern,classic,minimal,default";
}

String TemplateManager::replaceVariables(const String& html, const String& networks,
                                       const String& status, const String& title,
                                       const String& customParameters) const {
    String result = html;

    // Replace title
    result.replace("{{TITLE}}", title);

    // Replace networks
    result.replace("{{NETWORKS}}", _generateNetworkList(networks));

    // Replace status
    result.replace("{{STATUS}}", _generateStatusHTML(status));

    // Replace custom parameters
    result.replace("{{CUSTOM_PARAMETERS}}", customParameters);

    // Replace other common variables
    result.replace("{{VERSION}}", "1.0.0");
    result.replace("{{DEVICE_NAME}}", "Flexifi Device");

    return result;
}

// Private methods

String TemplateManager::_getBuiltinTemplate(const String& name) const {
    // Get template from embedded assets
    const char* templateData = FlexifiAssets::getTemplate(name.c_str());
    if (templateData) {
        String html = String(templateData);

        // Replace CSS and JS placeholders with embedded assets
        html = _injectEmbeddedAssets(html, name);

        return html;
    }

    // Fallback to default if template not found
    return _getDefaultTemplate();
}

String TemplateManager::_injectEmbeddedAssets(const String& html, const String& templateName) const {
    String result = html;

    // Inject CSS based on template name
    const char* cssData = FlexifiAssets::getCSS(templateName.c_str());
    if (cssData) {
        String cssContent = String(cssData);
        String upperTemplateName = templateName;
        upperTemplateName.toUpperCase();
        result.replace("{{CSS_" + upperTemplateName + "}}", cssContent);

        // Also handle generic CSS placeholder
        result.replace("{{CSS}}", cssContent);
    }

    // Inject JavaScript
    const char* jsData = FlexifiAssets::getJS("portal");
    if (jsData) {
        String jsContent = String(jsData);
        result.replace("{{JS_PORTAL}}", jsContent);
        result.replace("{{JS}}", jsContent);
    }

    return result;
}


String TemplateManager::_getDefaultTemplate() const {
    // Default to modern template using embedded assets
    return _getBuiltinTemplate("modern");
}

String TemplateManager::_processVariables(const String& html, const String& networks,
                                        const String& status, const String& title, const String& customParameters) const {
    return replaceVariables(html, networks, status, title, customParameters);
}

String TemplateManager::_generateNetworkList(const String& networksJSON) const {
    if (networksJSON.isEmpty() || networksJSON == "[]") {
        return "<p>No networks found. Click 'Scan Networks' to search for available WiFi networks.</p>";
    }

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, networksJSON);

    if (error) {
        FLEXIFI_LOGE("Failed to parse networks JSON: %s", error.c_str());
        return "<p>Error parsing network list</p>";
    }

    String html = "<div class=\"network-list\">";

    for (JsonVariant network : doc.as<JsonArray>()) {
        String ssid = network["ssid"].as<String>();
        int rssi = network["rssi"].as<int>();
        bool secure = network["secure"].as<bool>();

        String securityIcon = secure ? "🔒" : "🔓";
        String signalStrength = network["signal_strength"].as<String>();

        // Fallback signal strength calculation if not provided
        if (signalStrength.isEmpty()) {
            if (rssi > -50) {
                signalStrength = "📶📶📶📶";
            } else if (rssi > -60) {
                signalStrength = "📶📶📶";
            } else if (rssi > -70) {
                signalStrength = "📶📶";
            } else if (rssi > -80) {
                signalStrength = "📶";
            } else {
                signalStrength = "📵";
            }
        }

        html += "<div class=\"network-item\" onclick=\"selectNetwork('" +
                _escapeHTML(ssid) + "')\">";
        html += "<span class=\"network-name\">" + _escapeHTML(ssid) + "</span>";
        html += "<span class=\"network-info\">" + securityIcon + " " + signalStrength + "</span>";
        html += "</div>";
    }

    html += "</div>";
    return html;
}

String TemplateManager::_generateStatusHTML(const String& status) const {
    if (status == "scanning") {
        return "<div class=\"status scanning\">🔄 Scanning for networks...</div>";
    } else if (status == "connecting") {
        return "<div class=\"status connecting\">⏳ Connecting to network...</div>";
    } else if (status == "connected") {
        return "<div class=\"status connected\">✅ Connected successfully!</div>";
    } else if (status == "failed") {
        return "<div class=\"status failed\">❌ Connection failed</div>";
    } else {
        return "<div class=\"status ready\">🔧 Ready to configure</div>";
    }
}

String TemplateManager::_getModernCSS() const {
    return R"(
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
               background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
               min-height: 100vh; padding: 20px; }
        .container { max-width: 500px; margin: 0 auto; background: white;
                     border-radius: 12px; box-shadow: 0 10px 30px rgba(0,0,0,0.2);
                     overflow: hidden; }
        .header { background: #4a5568; color: white; padding: 30px; text-align: center; }
        .header h1 { font-size: 2em; margin-bottom: 10px; }
        .subtitle { opacity: 0.9; }
        .status-panel, .wifi-panel, .connect-panel, .actions { padding: 20px; }
        .status { padding: 15px; border-radius: 8px; margin-bottom: 20px; font-weight: 500; }
        .status.ready { background: #e6fffa; color: #065f46; }
        .status.scanning { background: #fef3c7; color: #92400e; }
        .status.connecting { background: #dbeafe; color: #1e40af; }
        .status.connected { background: #dcfce7; color: #166534; }
        .status.failed { background: #fee2e2; color: #dc2626; }
        .btn { padding: 12px 24px; border: none; border-radius: 6px; font-size: 14px;
               font-weight: 500; cursor: pointer; transition: all 0.2s; }
        .btn-primary { background: #3b82f6; color: white; }
        .btn-primary:hover { background: #2563eb; }
        .btn-secondary { background: #6b7280; color: white; }
        .btn-secondary:hover { background: #4b5563; }
        .btn-danger { background: #ef4444; color: white; }
        .btn-danger:hover { background: #dc2626; }
        .form-group { margin-bottom: 20px; }
        .form-group label { display: block; margin-bottom: 5px; font-weight: 500; }
        .form-group input { width: 100%; padding: 12px; border: 2px solid #e5e7eb;
                            border-radius: 6px; font-size: 16px; }
        .form-group input:focus { outline: none; border-color: #3b82f6; }
        .network-list { margin-top: 15px; }
        .network-item { padding: 12px; border: 1px solid #e5e7eb; border-radius: 6px;
                        margin-bottom: 8px; cursor: pointer; display: flex;
                        justify-content: space-between; align-items: center; }
        .network-item:hover { background: #f9fafb; }
        .network-name { font-weight: 500; }
        .network-info { color: #6b7280; }
        .actions { text-align: center; border-top: 1px solid #e5e7eb; }
    )";
}

String TemplateManager::_getClassicCSS() const {
    return R"(
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f5f5f5; }
        .container { max-width: 600px; margin: 0 auto; background: white;
                     padding: 20px; border-radius: 5px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; text-align: center; }
        h2 { color: #666; border-bottom: 2px solid #eee; padding-bottom: 10px; }
        .panel { margin-bottom: 20px; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }
        button { padding: 10px 20px; background: #007cba; color: white; border: none;
                 border-radius: 3px; cursor: pointer; }
        button:hover { background: #005a8b; }
        input { padding: 8px; margin: 5px 0; border: 1px solid #ddd; border-radius: 3px; }
        #status { padding: 10px; margin: 10px 0; border-radius: 3px; background: #f0f0f0; }
        .network-item { padding: 10px; margin: 5px 0; border: 1px solid #eee;
                        border-radius: 3px; cursor: pointer; }
        .network-item:hover { background: #f9f9f9; }
    )";
}

String TemplateManager::_getMinimalCSS() const {
    return R"(
        body { font-family: Arial, sans-serif; margin: 20px; }
        button { padding: 5px 10px; margin: 5px; }
        input { padding: 5px; margin: 2px; }
        #status { padding: 5px; margin: 10px 0; background: #f0f0f0; }
        .network-item { padding: 5px; margin: 2px; border: 1px solid #ccc; cursor: pointer; }
        .network-item:hover { background: #f5f5f5; }
    )";
}

String TemplateManager::_getPortalJS() const {
    // Use embedded assets instead of inline JavaScript
    const char* jsData = FlexifiAssets::getJS("portal");
    if (jsData) {
        return String(jsData);
    }
    return "";
}

String TemplateManager::_escapeHTML(const String& text) const {
    String escaped = text;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&#39;");
    return escaped;
}

String TemplateManager::_sanitizeTemplate(const String& html) const {
    // Basic sanitization - remove potentially dangerous elements
    String sanitized = html;

    // Remove script tags (except our own portal script)
    int scriptStart = 0;
    while ((scriptStart = sanitized.indexOf("<script", scriptStart)) != -1) {
        int scriptEnd = sanitized.indexOf("</script>", scriptStart);
        if (scriptEnd != -1) {
            String scriptContent = sanitized.substring(scriptStart, scriptEnd + 9);
            if (scriptContent.indexOf("portal.js") == -1 &&
                scriptContent.indexOf("scanNetworks") == -1) {
                sanitized.remove(scriptStart, scriptEnd - scriptStart + 9);
            } else {
                scriptStart = scriptEnd + 9;
            }
        } else {
            break;
        }
    }

    return sanitized;
}
