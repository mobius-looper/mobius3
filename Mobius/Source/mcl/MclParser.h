/**
 * Parse an MCL script string into an MclScript object.
 */

#pragma once

#include <JuceHeader.h>

#include "../model/Binding.h"

class MclParser
{
  public:

    MclParser(class Provider* p);
    ~MclParser();
    
    MclScript* parse(juce::String src, class MclResult& result);

  private:

    class Provider* provider = nullptr;
    std::unique_ptr<class MclScript> script;
    class MclResult* result = nullptr;
    
    int lineNumber = 1;
    juce::String line;
    
    class MclSection* currentSection = nullptr;

    // parse state for sessions and overlays
    class MclScope* currentScope = nullptr;

    // parse state for bindings

    // defaults
    Binding::Trigger bindingTrigger = Binding::TriggerUnknown;
    int bindingChannel = 0;
    juce::String bindingScope;

    // positional columns
    int typeColumn = 0;
    int channelColumn = 0;
    int valueColumn = 0;
    int symbolColumn = 0;
    int scopeColumn = 0;
    
    void addError(juce::String err);
    bool hasErrors();
    
    class MclSection* getSection();
    class MclScope* getScope();
    bool isKeyword(juce::String token, const char* keyword);
    void parseLine();
    
    void parseSession(juce::StringArray& tokens);
    void parseOverlay(juce::StringArray& tokens);
    void parseSessionLine(juce::StringArray& tokens, bool isOverlay);
    void parseScope(juce::StringArray& tokens);
    int parseScopeId(juce::String token);
    void parseAssignment(juce::StringArray& tokens);
    int parseParameterOrdinal(class Symbol* s, class ParameterProperties* props,
                              juce::String svalue, bool& isRemove);
    void validateStructureReference(class Symbol* s, class ParameterProperties* props,
                                    juce::String svalue);
    
    void parseBinding(juce::StringArray& tokens);
    void parseBindingLine(juce::StringArray& tokens);
    void parseBindingDefault(juce::StringArray& tokens);
    void parseBindingColumns(juce::StringArray& tokens);
    int parseChannel(juce::String s);
    Binding::Trigger parseTrigger(juce::String s);
    int parseMidiValue(juce::String s);
    void parseBindingObject(juce::StringArray& tokens);
    bool validateSymbol(juce::String name);
    bool validateBindingScope(juce::String name);
};

        
