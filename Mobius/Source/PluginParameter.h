/**
 * Class wrapping the juce::AudioProcessParameter and providing services
 * for change detection.
 *
 * Consider just making this a subclass
 *
 * todo: Now that we force these to have Symbols, don't need the name too.
 * also make Symbol use juce::String instead of const char*
 */

#pragma once

#include <JuceHeader.h>

#include "model/UIAction.h"

class PluginParameter
{
  public:

    PluginParameter(class Symbol* s, class OldBinding* b);
    PluginParameter(class Symbol* s, class VariableDefinition* var);
    ~PluginParameter();

    juce::AudioProcessorParameter* getJuceParameter();

    juce::String getName() {
        return symbol->name;
    }

    bool capture();
    int get();
    void set(int neu);
    
    Symbol* symbol = nullptr;

    void setScope(const char* s);
    const char* getScope();

    // unique identifier used when the parameter is bound to a sustainable function
    // we're going to need something similar to tag AU parameters, can this be the same?
    int sustainId = 0;
    
    // test hack
    bool installed = false;
    
  private:

    char scope[UIActionScopeMax];
    
    // one of the various parameter types
    // don't need more than one of these but I don't want to mess with downcasting right now
    // Juce wraps convenient range normalization these
    juce::AudioParameterInt* intParameter = nullptr;
    juce::AudioParameterBool* boolParameter = nullptr;
    juce::AudioParameterChoice* choiceParameter = nullptr;

    // the last normalized value
    int last = 0;
    int min = 0;
    int max = 0;
    
    int getCurrent();
};

