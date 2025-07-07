#include "web_assets.h"
#include <string.h>

namespace FlexifiAssets {

const char* getTemplate(const char* name) {
    if (strcmp(name, "modern") == 0) return template_modern;
    if (strcmp(name, "classic") == 0) return template_classic;
    if (strcmp(name, "minimal") == 0) return template_minimal;
    return nullptr;
}

const char* getCSS(const char* name) {
    if (strcmp(name, "classic") == 0) return css_classic;
    if (strcmp(name, "minimal") == 0) return css_minimal;
    if (strcmp(name, "modern") == 0) return css_modern;
    return nullptr;
}

const char* getJS(const char* name) {
    if (strcmp(name, "portal") == 0) return js_portal;
    return nullptr;
}

// Size functions
size_t getTemplateSize(const char* name) {
    if (strcmp(name, "modern") == 0) return template_modern_len;
    if (strcmp(name, "classic") == 0) return template_classic_len;
    if (strcmp(name, "minimal") == 0) return template_minimal_len;
    return 0;
}

size_t getCSSSize(const char* name) {
    if (strcmp(name, "classic") == 0) return css_classic_len;
    if (strcmp(name, "minimal") == 0) return css_minimal_len;
    if (strcmp(name, "modern") == 0) return css_modern_len;
    return 0;
}

size_t getJSSize(const char* name) {
    if (strcmp(name, "portal") == 0) return js_portal_len;
    return 0;
}

} // namespace FlexifiAssets
