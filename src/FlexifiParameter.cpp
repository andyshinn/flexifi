#include "FlexifiParameter.h"

FlexifiParameter::FlexifiParameter(const String& id, const String& label, const String& defaultValue, 
                                  int maxLength, ParameterType type) :
    _id(id),
    _label(label),
    _value(defaultValue),
    _defaultValue(defaultValue),
    _placeholder(""),
    _customHTML(""),
    _options(nullptr),
    _maxLength(maxLength),
    _optionCount(0),
    _type(type),
    _required(false) {
}

FlexifiParameter::FlexifiParameter(const String& id, const String& label, const String& defaultValue, 
                                  int maxLength, const String& customHTML) :
    _id(id),
    _label(label),
    _value(defaultValue),
    _defaultValue(defaultValue),
    _placeholder(""),
    _customHTML(customHTML),
    _options(nullptr),
    _maxLength(maxLength),
    _optionCount(0),
    _type(ParameterType::TEXT),
    _required(false) {
}

FlexifiParameter::FlexifiParameter(const String& id, const String& label, const String& defaultValue,
                                  const String* options, int optionCount) :
    _id(id),
    _label(label),
    _value(defaultValue),
    _defaultValue(defaultValue),
    _placeholder(""),
    _customHTML(""),
    _options(nullptr),
    _maxLength(100),
    _optionCount(optionCount),
    _type(ParameterType::SELECT),
    _required(false) {
    
    if (options && optionCount > 0) {
        _copyOptions(options, optionCount);
    }
}

FlexifiParameter::FlexifiParameter(const FlexifiParameter& other) :
    _id(other._id),
    _label(other._label),
    _value(other._value),
    _defaultValue(other._defaultValue),
    _placeholder(other._placeholder),
    _customHTML(other._customHTML),
    _options(nullptr),
    _maxLength(other._maxLength),
    _optionCount(other._optionCount),
    _type(other._type),
    _required(other._required) {
    
    if (other._options && other._optionCount > 0) {
        _copyOptions(other._options, other._optionCount);
    }
}

FlexifiParameter& FlexifiParameter::operator=(const FlexifiParameter& other) {
    if (this != &other) {
        _clearOptions();
        
        _id = other._id;
        _label = other._label;
        _value = other._value;
        _defaultValue = other._defaultValue;
        _placeholder = other._placeholder;
        _customHTML = other._customHTML;
        _maxLength = other._maxLength;
        _optionCount = other._optionCount;
        _type = other._type;
        _required = other._required;
        
        if (other._options && other._optionCount > 0) {
            _copyOptions(other._options, other._optionCount);
        }
    }
    return *this;
}

FlexifiParameter::~FlexifiParameter() {
    _clearOptions();
}

String FlexifiParameter::getID() const {
    return _id;
}

String FlexifiParameter::getLabel() const {
    return _label;
}

String FlexifiParameter::getValue() const {
    return _value;
}

String FlexifiParameter::getDefaultValue() const {
    return _defaultValue;
}

int FlexifiParameter::getMaxLength() const {
    return _maxLength;
}

ParameterType FlexifiParameter::getType() const {
    return _type;
}

String FlexifiParameter::getCustomHTML() const {
    return _customHTML;
}

String* FlexifiParameter::getOptions() const {
    return _options;
}

int FlexifiParameter::getOptionCount() const {
    return _optionCount;
}

String FlexifiParameter::getPlaceholder() const {
    return _placeholder;
}

bool FlexifiParameter::isRequired() const {
    return _required;
}

void FlexifiParameter::setValue(const String& value) {
    _value = value;
}

void FlexifiParameter::setPlaceholder(const String& placeholder) {
    _placeholder = placeholder;
}

void FlexifiParameter::setRequired(bool required) {
    _required = required;
}

void FlexifiParameter::setCustomHTML(const String& html) {
    _customHTML = html;
}

String FlexifiParameter::generateHTML() const {
    if (!_customHTML.isEmpty()) {
        return _customHTML;
    }
    
    String html = "<div class=\"form-group\">";
    html += generateLabel();
    html += generateInput();
    html += "</div>";
    
    return html;
}

String FlexifiParameter::generateLabel() const {
    String html = "<label for=\"" + _id + "\">" + _escapeHTML(_label);
    if (_required) {
        html += " <span class=\"required\">*</span>";
    }
    html += "</label>";
    return html;
}

String FlexifiParameter::generateInput() const {
    switch (_type) {
        case ParameterType::SELECT:
            return _generateSelectHTML();
        case ParameterType::TEXTAREA:
            return _generateTextareaHTML();
        case ParameterType::CHECKBOX:
            return _generateCheckboxHTML();
        default:
            return _generateTextHTML();
    }
}

bool FlexifiParameter::validate() const {
    if (_required && _value.isEmpty()) {
        return false;
    }
    
    if (_value.length() > _maxLength) {
        return false;
    }
    
    // Type-specific validation
    switch (_type) {
        case ParameterType::EMAIL:
            return _value.isEmpty() || _value.indexOf("@") > 0;
        case ParameterType::NUMBER:
            for (unsigned int i = 0; i < _value.length(); i++) {
                if (!isdigit(_value[i]) && _value[i] != '.' && _value[i] != '-') {
                    return false;
                }
            }
            break;
        case ParameterType::URL:
            return _value.isEmpty() || _value.startsWith("http://") || _value.startsWith("https://");
        default:
            break;
    }
    
    return true;
}

String FlexifiParameter::getValidationError() const {
    if (_required && _value.isEmpty()) {
        return _label + " is required";
    }
    
    if (_value.length() > _maxLength) {
        return _label + " must be " + String(_maxLength) + " characters or less";
    }
    
    switch (_type) {
        case ParameterType::EMAIL:
            if (!_value.isEmpty() && _value.indexOf("@") <= 0) {
                return _label + " must be a valid email address";
            }
            break;
        case ParameterType::NUMBER:
            for (unsigned int i = 0; i < _value.length(); i++) {
                if (!isdigit(_value[i]) && _value[i] != '.' && _value[i] != '-') {
                    return _label + " must be a valid number";
                }
            }
            break;
        case ParameterType::URL:
            if (!_value.isEmpty() && !_value.startsWith("http://") && !_value.startsWith("https://")) {
                return _label + " must be a valid URL";
            }
            break;
        default:
            break;
    }
    
    return "";
}

// Private methods

void FlexifiParameter::_copyOptions(const String* options, int count) {
    _clearOptions();
    if (options && count > 0) {
        _options = new String[count];
        _optionCount = count;
        for (int i = 0; i < count; i++) {
            _options[i] = options[i];
        }
    }
}

void FlexifiParameter::_clearOptions() {
    if (_options) {
        delete[] _options;
        _options = nullptr;
        _optionCount = 0;
    }
}

String FlexifiParameter::_escapeHTML(const String& text) const {
    String escaped = text;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&#39;");
    return escaped;
}

String FlexifiParameter::_getTypeString() const {
    switch (_type) {
        case ParameterType::PASSWORD: return "password";
        case ParameterType::NUMBER: return "number";
        case ParameterType::EMAIL: return "email";
        case ParameterType::URL: return "url";
        case ParameterType::HIDDEN: return "hidden";
        default: return "text";
    }
}

String FlexifiParameter::_generateSelectHTML() const {
    String html = "<select id=\"" + _id + "\" name=\"" + _id + "\"";
    if (_required) html += " required";
    html += ">";
    
    if (!_required) {
        html += "<option value=\"\">-- Select --</option>";
    }
    
    for (int i = 0; i < _optionCount; i++) {
        html += "<option value=\"" + _escapeHTML(_options[i]) + "\"";
        if (_value == _options[i]) {
            html += " selected";
        }
        html += ">" + _escapeHTML(_options[i]) + "</option>";
    }
    
    html += "</select>";
    return html;
}

String FlexifiParameter::_generateTextHTML() const {
    String html = "<input type=\"" + _getTypeString() + "\" id=\"" + _id + "\" name=\"" + _id + "\"";
    html += " value=\"" + _escapeHTML(_value) + "\"";
    
    if (_maxLength > 0) {
        html += " maxlength=\"" + String(_maxLength) + "\"";
    }
    
    if (!_placeholder.isEmpty()) {
        html += " placeholder=\"" + _escapeHTML(_placeholder) + "\"";
    }
    
    if (_required) {
        html += " required";
    }
    
    html += ">";
    return html;
}

String FlexifiParameter::_generateCheckboxHTML() const {
    String html = "<input type=\"checkbox\" id=\"" + _id + "\" name=\"" + _id + "\" value=\"1\"";
    
    if (_value == "1" || _value.equalsIgnoreCase("true") || _value.equalsIgnoreCase("yes")) {
        html += " checked";
    }
    
    html += "> " + _escapeHTML(_label);
    return html;
}

String FlexifiParameter::_generateTextareaHTML() const {
    String html = "<textarea id=\"" + _id + "\" name=\"" + _id + "\"";
    
    if (_maxLength > 0) {
        html += " maxlength=\"" + String(_maxLength) + "\"";
    }
    
    if (!_placeholder.isEmpty()) {
        html += " placeholder=\"" + _escapeHTML(_placeholder) + "\"";
    }
    
    if (_required) {
        html += " required";
    }
    
    html += " rows=\"3\">" + _escapeHTML(_value) + "</textarea>";
    return html;
}