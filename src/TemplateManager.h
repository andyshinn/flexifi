#ifndef TEMPLATEMANAGER_H
#define TEMPLATEMANAGER_H

#include <Arduino.h>

class TemplateManager {
public:
    TemplateManager();
    ~TemplateManager();

    // Template management
    void setTemplate(const String& templateName);
    void setCustomTemplate(const String& htmlTemplate);
    String getCurrentTemplate() const;

    // HTML generation
    String getPortalHTML(const String& customParameters = "") const;
    String processTemplate(const String& templateStr, const String& networks,
                          const String& customParameters = "") const;

    // Template validation
    bool isValidTemplate(const String& templateName) const;
    String getAvailableTemplates() const;

    // Variable replacement
    String replaceVariables(const String& html, const String& networks, 
                           const String& status = "", const String& title = "Flexifi Setup",
                           const String& customParameters = "") const;

private:
    String _currentTemplate;
    String _customTemplate;
    bool _usingCustomTemplate;

    // Built-in templates
    String _getBuiltinTemplate(const String& name) const;
    String _getModernTemplate() const;
    String _getClassicTemplate() const;
    String _getMinimalTemplate() const;
    String _getDefaultTemplate() const;

    // Template processing
    String _processVariables(const String& html, const String& networks, 
                            const String& status, const String& title,
                            const String& customParameters = "") const;
    String _generateNetworkList(const String& networksJSON) const;
    String _generateStatusHTML(const String& status) const;

    // Legacy CSS and JavaScript methods (deprecated - now using embedded assets)
    String _getModernCSS() const;
    String _getClassicCSS() const;
    String _getMinimalCSS() const;
    String _getPortalJS() const;

    // Asset injection
    String _injectEmbeddedAssets(const String& html, const String& templateName) const;
    
    // Utility methods
    String _escapeHTML(const String& text) const;
    String _sanitizeTemplate(const String& html) const;
};

#endif // TEMPLATEMANAGER_H