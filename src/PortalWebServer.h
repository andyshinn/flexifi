#ifndef PORTALWEBSERVER_H
#define PORTALWEBSERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>

// Forward declaration
class Flexifi;

class PortalWebServer {
public:
    PortalWebServer(AsyncWebServer* server, Flexifi* portal);
    ~PortalWebServer();

    // Server management
    bool init();
    void setupRoutes();
    void setupWebSocket();
    void cleanup();

    // WebSocket communication
    void broadcastStatus(const String& message);
    void broadcastNetworks(const String& networksJSON);
    void broadcastMessage(const String& type, const String& data);
    size_t getWebSocketClientCount() const;

    // Route handlers
    void handleRoot(AsyncWebServerRequest* request);
    void handleScan(AsyncWebServerRequest* request);
    void handleConnect(AsyncWebServerRequest* request);
    void handleStatus(AsyncWebServerRequest* request);
    void handleReset(AsyncWebServerRequest* request);
    void handleNetworksJSON(AsyncWebServerRequest* request);
    void handleNotFound(AsyncWebServerRequest* request);
    // handleCaptivePortalDetect removed - using 404 handler approach instead

    // WebSocket event handler
    void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                         AwsEventType type, void* arg, uint8_t* data, size_t len);

    // Utility methods
    bool isInitialized() const;
    String getServerInfo() const;

private:
    AsyncWebServer* _server;
    AsyncWebSocket* _ws;
    Flexifi* _portal;
    bool _initialized;
    bool _routesSetup;
    size_t _clientCount;

    // WebSocket message handling
    void _handleWebSocketMessage(AsyncWebSocketClient* client, 
                                const String& message);
    void _sendWebSocketMessage(AsyncWebSocketClient* client, 
                              const String& message);
    void _broadcastToAllClients(const String& message);

    // JSON response helpers
    String _createJSONResponse(bool success, const String& message, 
                              const String& data = "");
    String _createStatusJSON(const String& status, const String& message);
    String _createNetworksJSON(const String& networks);

    // Request validation
    bool _validateRequest(AsyncWebServerRequest* request);
    bool _validateConnectRequest(AsyncWebServerRequest* request);
    String _sanitizeInput(const String& input);

    // CORS and security headers
    void _setCORSHeaders(AsyncWebServerResponse* response);
    void _setSecurityHeaders(AsyncWebServerResponse* response);
    void _setContentTypeHeaders(AsyncWebServerResponse* response, 
                               const String& contentType);

    // Error handling
    void _sendError(AsyncWebServerRequest* request, int code, 
                   const String& message);
    void _sendJSON(AsyncWebServerRequest* request, const String& json);
    void _sendHTML(AsyncWebServerRequest* request, const String& html);
};

#endif // PORTALWEBSERVER_H