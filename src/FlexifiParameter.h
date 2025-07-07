#ifndef FLEXIFIPARAMETER_H
#define FLEXIFIPARAMETER_H

#include <Arduino.h>

enum class ParameterType {
    TEXT,
    PASSWORD,
    NUMBER,
    EMAIL,
    URL,
    TEXTAREA,
    SELECT,
    CHECKBOX,
    HIDDEN
};

class FlexifiParameter {
public:
    // Basic constructor
    FlexifiParameter(const String& id, const String& label, const String& defaultValue = "", 
                    int maxLength = 40, ParameterType type = ParameterType::TEXT);
    
    // Advanced constructor with custom HTML
    FlexifiParameter(const String& id, const String& label, const String& defaultValue, 
                    int maxLength, const String& customHTML);
    
    // Select dropdown constructor
    FlexifiParameter(const String& id, const String& label, const String& defaultValue,
                    const String* options, int optionCount);
    
    // Copy constructor and assignment
    FlexifiParameter(const FlexifiParameter& other);
    FlexifiParameter& operator=(const FlexifiParameter& other);
    
    ~FlexifiParameter();

    // Getters
    String getID() const;
    String getLabel() const;
    String getValue() const;
    String getDefaultValue() const;
    int getMaxLength() const;
    ParameterType getType() const;
    String getCustomHTML() const;
    String* getOptions() const;
    int getOptionCount() const;
    String getPlaceholder() const;
    bool isRequired() const;

    // Setters
    void setValue(const String& value);
    void setPlaceholder(const String& placeholder);
    void setRequired(bool required);
    void setCustomHTML(const String& html);
    
    // HTML generation
    String generateHTML() const;
    String generateLabel() const;
    String generateInput() const;
    
    // Validation
    bool validate() const;
    String getValidationError() const;

private:
    String _id;
    String _label;
    String _value;
    String _defaultValue;
    String _placeholder;
    String _customHTML;
    String* _options;
    int _maxLength;
    int _optionCount;
    ParameterType _type;
    bool _required;
    
    // Helper methods
    void _copyOptions(const String* options, int count);
    void _clearOptions();
    String _escapeHTML(const String& text) const;
    String _getTypeString() const;
    String _generateSelectHTML() const;
    String _generateTextHTML() const;
    String _generateCheckboxHTML() const;
    String _generateTextareaHTML() const;
};

#endif // FLEXIFIPARAMETER_H