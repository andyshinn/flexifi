#include "PortalWebServer.h"
#include "Flexifi.h"
#include <ArduinoJson.h>

PortalWebServer::PortalWebServer(AsyncWebServer* server, Flexifi* portal) :
    _server(server),
    _ws(nullptr),
    _portal(portal),
    _initialized(false),
    _routesSetup(false),
    _clientCount(0) {
}

PortalWebServer::~PortalWebServer() {
    cleanup();
}

bool PortalWebServer::init() {
    if (_initialized) {
        FLEXIFI_LOGW("PortalWebServer already initialized");
        return true;
    }

    if (!_server) {
        FLEXIFI_LOGE("AsyncWebServer pointer is null");
        return false;
    }

    if (!_portal) {
        FLEXIFI_LOGE("Flexifi portal pointer is null");
        return false;
    }

    FLEXIFI_LOGI("Initializing PortalWebServer");

    // Set up WebSocket
    setupWebSocket();

    // Set up routes
    setupRoutes();

    _initialized = true;
    FLEXIFI_LOGI("PortalWebServer initialized successfully");
    return true;
}

void PortalWebServer::setupRoutes() {
    if (_routesSetup) {
        FLEXIFI_LOGW("Routes already set up");
        return;
    }

    FLEXIFI_LOGD("Setting up HTTP routes");

    // Main portal page
    _server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleRoot(request);
    });

    // Note: Captive portal detection is now handled by the 404 handler
    // This is more effective than specific routes as it catches all detection attempts
    
    // Manual captive portal trigger - users can navigate to this if auto-detection fails
    _server->on("/portal", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleRoot(request);
    });

    // API endpoints
    _server->on("/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleScan(request);
    });

    _server->on("/connect", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleConnect(request);
    });

    _server->on("/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleStatus(request);
    });

    _server->on("/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleReset(request);
    });

    _server->on("/networks.json", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleNetworksJSON(request);
    });

    // Handle 404
    _server->onNotFound([this](AsyncWebServerRequest* request) {
        handleNotFound(request);
    });

    _routesSetup = true;
    FLEXIFI_LOGD("HTTP routes set up successfully");
}

void PortalWebServer::setupWebSocket() {
    if (_ws) {
        FLEXIFI_LOGW("WebSocket already set up");
        return;
    }

#ifndef FLEXIFI_DISABLE_WEBSOCKET
    FLEXIFI_LOGD("Setting up WebSocket");

    _ws = new AsyncWebSocket("/ws");
    _ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
        onWebSocketEvent(server, client, type, arg, data, len);
    });

    _server->addHandler(_ws);
    FLEXIFI_LOGD("WebSocket set up successfully");
#else
    FLEXIFI_LOGI("WebSocket support disabled");
#endif
}

void PortalWebServer::cleanup() {
    if (_ws) {
        _ws->closeAll();
        delete _ws;
        _ws = nullptr;
    }

    _initialized = false;
    _routesSetup = false;
    _clientCount = 0;

    FLEXIFI_LOGD("PortalWebServer cleaned up");
}

void PortalWebServer::broadcastStatus(const String& message) {
#ifndef FLEXIFI_DISABLE_WEBSOCKET
    if (_ws && _clientCount > 0) {
        String json = _createStatusJSON("update", message);
        _broadcastToAllClients(json);
        FLEXIFI_LOGD("Status broadcast: %s", message.c_str());
    }
#endif
}

void PortalWebServer::broadcastNetworks(const String& networksJSON) {
#ifndef FLEXIFI_DISABLE_WEBSOCKET
    if (_ws && _clientCount > 0) {
        DynamicJsonDocument doc(1024);
        doc["type"] = "scan_complete";
        doc["data"]["networks"] = serialized(networksJSON);
        
        String message;
        serializeJson(doc, message);
        _broadcastToAllClients(message);
        FLEXIFI_LOGD("Networks broadcast sent");
    }
#endif
}

void PortalWebServer::broadcastMessage(const String& type, const String& data) {
#ifndef FLEXIFI_DISABLE_WEBSOCKET
    if (_ws && _clientCount > 0) {
        DynamicJsonDocument doc(1024);
        doc["type"] = type;
        doc["data"] = data;
        
        String message;
        serializeJson(doc, message);
        _broadcastToAllClients(message);
        FLEXIFI_LOGD("Message broadcast: %s", type.c_str());
    }
#endif
}

size_t PortalWebServer::getWebSocketClientCount() const {
    return _clientCount;
}

void PortalWebServer::handleRoot(AsyncWebServerRequest* request) {
    FLEXIFI_LOGD("Handling root request from %s", request->client()->remoteIP().toString().c_str());
    
    if (!_validateRequest(request)) {
        _sendError(request, 400, "Invalid request");
        return;
    }

    // Get HTML from template manager
    String html = _portal->getPortalHTML();
    
    if (html.isEmpty()) {
        _sendError(request, 500, "Failed to generate portal HTML");
        return;
    }

    AsyncWebServerResponse* response = request->beginResponse(200, "text/html", html);
    _setSecurityHeaders(response);
    _setCORSHeaders(response);
    request->send(response);
}

void PortalWebServer::handleScan(AsyncWebServerRequest* request) {
    FLEXIFI_LOGD("Handling scan request");
    
    if (!_validateRequest(request)) {
        _sendError(request, 400, "Invalid request");
        return;
    }

    // Trigger network scan
    bool scanStarted = _portal->scanNetworks();
    
    if (!scanStarted) {
        // Scan was throttled, calculate remaining time
        unsigned long timeRemaining = _portal->getScanTimeRemaining();
        String throttleMessage = "Scan throttled. Please wait " + String(timeRemaining / 1000) + " more seconds.";
        _sendJSON(request, _createJSONResponse(false, throttleMessage));
    } else {
        // Return current networks (may be empty if scan just started)
        String networksJSON = _portal->getNetworksJSON();
        _sendJSON(request, _createJSONResponse(true, "Scan initiated", networksJSON));
    }
}

void PortalWebServer::handleConnect(AsyncWebServerRequest* request) {
    FLEXIFI_LOGD("Handling connect request");
    
    if (!_validateConnectRequest(request)) {
        _sendError(request, 400, "Invalid connect request");
        return;
    }

    String ssid = "";
    String password = "";

    // Get parameters from POST body
    if (request->hasParam("ssid", true)) {
        ssid = request->getParam("ssid", true)->value();
    }
    if (request->hasParam("password", true)) {
        password = request->getParam("password", true)->value();
    }

    // Sanitize inputs
    ssid = _sanitizeInput(ssid);
    password = _sanitizeInput(password);

    if (ssid.isEmpty()) {
        _sendJSON(request, _createJSONResponse(false, "SSID cannot be empty"));
        return;
    }

    // Process custom parameters - iterate through all request parameters
    for (int i = 0; i < request->params(); i++) {
        const AsyncWebParameter* param = request->getParam(i);
        if (param && param->isPost() && param->name() != "ssid" && param->name() != "password") {
            // This is a custom parameter
            String paramName = param->name();
            String paramValue = _sanitizeInput(param->value());
            _portal->setParameterValue(paramName, paramValue);
            FLEXIFI_LOGD("Custom parameter %s = %s", paramName.c_str(), paramValue.c_str());
        }
    }

    FLEXIFI_LOGI("Connection request for SSID: %s", ssid.c_str());

    // Attempt connection
    bool success = _portal->connectToWiFi(ssid, password);
    
    if (success) {
        _sendJSON(request, _createJSONResponse(true, "Connection initiated"));
    } else {
        _sendJSON(request, _createJSONResponse(false, "Failed to initiate connection"));
    }
}

void PortalWebServer::handleStatus(AsyncWebServerRequest* request) {
    FLEXIFI_LOGD("Handling status request");
    
    if (!_validateRequest(request)) {
        _sendError(request, 400, "Invalid request");
        return;
    }

    String statusJSON = _portal->getStatusJSON();
    _sendJSON(request, statusJSON);
}

void PortalWebServer::handleReset(AsyncWebServerRequest* request) {
    FLEXIFI_LOGD("Handling reset request");
    
    if (!_validateRequest(request)) {
        _sendError(request, 400, "Invalid request");
        return;
    }

    _portal->reset();
    _sendJSON(request, _createJSONResponse(true, "Configuration reset"));
}

void PortalWebServer::handleNetworksJSON(AsyncWebServerRequest* request) {
    FLEXIFI_LOGD("Handling networks.json request");
    
    if (!_validateRequest(request)) {
        _sendError(request, 400, "Invalid request");
        return;
    }

    String networksArray = _portal->getNetworksJSON();
    
    // Wrap the networks array in the expected format {networks: [...]}
    DynamicJsonDocument doc(3072); // Increased size to accommodate wrapper
    doc["networks"] = serialized(networksArray);
    
    String response;
    serializeJson(doc, response);
    _sendJSON(request, response);
}

void PortalWebServer::handleNotFound(AsyncWebServerRequest* request) {
    FLEXIFI_LOGD("Handling 404 for: %s (Host: %s)", request->url().c_str(), request->host().c_str());
    
    // WiFiManager-style captive portal: redirect ALL requests to different hosts
    String serverIP = WiFi.softAPIP().toString();
    String requestHost = request->host();
    
    // Check if this request is for a different host than our AP IP
    bool shouldRedirect = (requestHost != serverIP);
    
    // Also redirect if the request is for any detection URL (be more aggressive)
    String url = request->url();
    if (url.indexOf("generate_204") >= 0 || 
        url.indexOf("connecttest") >= 0 || 
        url.indexOf("hotspot-detect") >= 0 ||
        url.indexOf("success") >= 0 ||
        url.indexOf("ncsi") >= 0 ||
        url.indexOf("canonical") >= 0 ||
        url.indexOf("library/test") >= 0) {
        shouldRedirect = true;
    }
    
    if (shouldRedirect) {
        // Redirect to captive portal
        String redirectUrl = "http://" + serverIP;
        FLEXIFI_LOGI("🔄 Captive Portal Redirect: %s (host: %s) → %s", url.c_str(), requestHost.c_str(), redirectUrl.c_str());
        request->redirect(redirectUrl);
    } else {
        // Local request for our IP, serve the portal page
        FLEXIFI_LOGD("📄 Serving portal page for local request: %s", url.c_str());
        handleRoot(request);
    }
}

// Note: handleCaptivePortalDetect removed - all captive portal detection 
// is now handled by the more effective 404 handler approach

void PortalWebServer::onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
#ifndef FLEXIFI_DISABLE_WEBSOCKET
    switch (type) {
        case WS_EVT_CONNECT:
            FLEXIFI_LOGD("WebSocket client connected: %u", client->id());
            _clientCount++;
            break;
            
        case WS_EVT_DISCONNECT:
            FLEXIFI_LOGD("WebSocket client disconnected: %u", client->id());
            if (_clientCount > 0) _clientCount--;
            break;
            
        case WS_EVT_DATA: {
            AwsFrameInfo* info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                String message = String((char*)data, len);
                FLEXIFI_LOGD("WebSocket message received: %s", message.c_str());
                _handleWebSocketMessage(client, message);
            }
            break;
        }
            
        case WS_EVT_PONG:
            FLEXIFI_LOGD("WebSocket pong received: %u", client->id());
            break;
            
        case WS_EVT_ERROR:
            FLEXIFI_LOGW("WebSocket error: %u", client->id());
            break;
    }
#endif
}

bool PortalWebServer::isInitialized() const {
    return _initialized;
}

String PortalWebServer::getServerInfo() const {
    String info = "PortalWebServer: ";
    info += _initialized ? "Initialized" : "Not initialized";
    info += ", Clients: " + String(_clientCount);
    info += ", Routes: " + String(_routesSetup ? "Set up" : "Not set up");
    return info;
}

// Private methods

void PortalWebServer::_handleWebSocketMessage(AsyncWebSocketClient* client, const String& message) {
#ifndef FLEXIFI_DISABLE_WEBSOCKET
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
        FLEXIFI_LOGW("Invalid WebSocket JSON: %s", error.c_str());
        return;
    }
    
    String action = doc["action"];
    
    if (action == "scan") {
        bool scanStarted = _portal->scanNetworks();
        
        if (!scanStarted) {
            // Scan was throttled, calculate remaining time
            unsigned long timeRemaining = _portal->getScanTimeRemaining();
            String throttleMessage = "Scan throttled. Please wait " + String(timeRemaining / 1000) + " more seconds.";
            _sendWebSocketMessage(client, _createJSONResponse(false, throttleMessage));
        } else {
            _sendWebSocketMessage(client, _createJSONResponse(true, "Scan initiated"));
        }
    } else if (action == "connect") {
        String ssid = doc["ssid"];
        String password = doc["password"];
        
        if (ssid.isEmpty()) {
            _sendWebSocketMessage(client, _createJSONResponse(false, "SSID required"));
            return;
        }
        
        bool success = _portal->connectToWiFi(ssid, password);
        _sendWebSocketMessage(client, _createJSONResponse(success, 
            success ? "Connection initiated" : "Failed to initiate connection"));
    } else if (action == "status") {
        String statusJSON = _portal->getStatusJSON();
        _sendWebSocketMessage(client, statusJSON);
    } else if (action == "reset") {
        _portal->reset();
        _sendWebSocketMessage(client, _createJSONResponse(true, "Configuration reset"));
    } else {
        _sendWebSocketMessage(client, _createJSONResponse(false, "Unknown action"));
    }
#endif
}

void PortalWebServer::_sendWebSocketMessage(AsyncWebSocketClient* client, const String& message) {
#ifndef FLEXIFI_DISABLE_WEBSOCKET
    if (client && client->status() == WS_CONNECTED) {
        client->text(message);
    }
#endif
}

void PortalWebServer::_broadcastToAllClients(const String& message) {
#ifndef FLEXIFI_DISABLE_WEBSOCKET
    if (_ws) {
        _ws->textAll(message);
    }
#endif
}

String PortalWebServer::_createJSONResponse(bool success, const String& message, const String& data) {
    DynamicJsonDocument doc(1024);
    doc["success"] = success;
    doc["message"] = message;
    
    if (!data.isEmpty()) {
        doc["data"] = serialized(data);
    }
    
    String response;
    serializeJson(doc, response);
    return response;
}

String PortalWebServer::_createStatusJSON(const String& status, const String& message) {
    DynamicJsonDocument doc(512);
    doc["type"] = "status_update";
    doc["data"]["status"] = status;
    doc["data"]["message"] = message;
    
    String response;
    serializeJson(doc, response);
    return response;
}

String PortalWebServer::_createNetworksJSON(const String& networks) {
    DynamicJsonDocument doc(1024);
    doc["type"] = "scan_complete";
    doc["data"]["networks"] = serialized(networks);
    
    String response;
    serializeJson(doc, response);
    return response;
}

bool PortalWebServer::_validateRequest(AsyncWebServerRequest* request) {
    if (!request) {
        return false;
    }
    
    // Basic validation - can be extended
    return true;
}

bool PortalWebServer::_validateConnectRequest(AsyncWebServerRequest* request) {
    if (!_validateRequest(request)) {
        return false;
    }
    
    if (request->method() != HTTP_POST) {
        return false;
    }
    
    return true;
}

String PortalWebServer::_sanitizeInput(const String& input) {
    String sanitized = input;
    
    // Remove control characters
    sanitized.replace("\r", "");
    sanitized.replace("\n", "");
    sanitized.replace("\t", "");
    
    // Trim whitespace
    sanitized.trim();
    
    return sanitized;
}

void PortalWebServer::_setCORSHeaders(AsyncWebServerResponse* response) {
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

void PortalWebServer::_setSecurityHeaders(AsyncWebServerResponse* response) {
    response->addHeader("X-Content-Type-Options", "nosniff");
    response->addHeader("X-Frame-Options", "DENY");
    response->addHeader("X-XSS-Protection", "1; mode=block");
}

void PortalWebServer::_setContentTypeHeaders(AsyncWebServerResponse* response, const String& contentType) {
    response->addHeader("Content-Type", contentType);
}

void PortalWebServer::_sendError(AsyncWebServerRequest* request, int code, const String& message) {
    String json = _createJSONResponse(false, message);
    AsyncWebServerResponse* response = request->beginResponse(code, "application/json", json);
    _setSecurityHeaders(response);
    _setCORSHeaders(response);
    request->send(response);
}

void PortalWebServer::_sendJSON(AsyncWebServerRequest* request, const String& json) {
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", json);
    _setSecurityHeaders(response);
    _setCORSHeaders(response);
    request->send(response);
}

void PortalWebServer::_sendHTML(AsyncWebServerRequest* request, const String& html) {
    AsyncWebServerResponse* response = request->beginResponse(200, "text/html", html);
    _setSecurityHeaders(response);
    _setCORSHeaders(response);
    request->send(response);
}