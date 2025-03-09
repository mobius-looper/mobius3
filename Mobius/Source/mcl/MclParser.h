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
    class MclObjectScope* currentObject = nullptr;
    class MclRunningScope* currentScope = nullptr;

    void parseLine(juce::String line);
    class MclObjectScope* getObject();
    class MclRunningScope* getScope();
    void addError(juce::String line, juce::String err);
    bool hasErrors();
    
};

        
