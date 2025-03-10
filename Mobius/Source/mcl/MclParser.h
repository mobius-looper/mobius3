/**
 * Parse an MCL script string into an MclScript object.
 */

#pragma once

#include <JuceHeader.h>

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
    class MclScope* currentScope = nullptr;

    void addError(juce::String err);
    bool hasErrors();
    
    class MclSection* getSection();
    class MclScope* getScope();
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
    
};

        
