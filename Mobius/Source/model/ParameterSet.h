
#pragma once

class ParameterSet
{
  public:

    ParameterSet() {}
    ~ParameterSet() {}

    // standard set names
    const char* GlobalSet = "Global";
    const char* DefaultSet = "Default";

    /**
     * All parameter sets have names.
     */
    juce::String name;
    juce::OwnedArray<ParameterValue> parameters;

    // resolved symbol after loading
    class Symbol* symbol = nullptr;
    
};

class ParameterValue
{
  public:

    ParameterValue() {}
    ~ParameterValue() {}

    int value = 0;

    // for the rare cases where we have string values
    char string[256];

    bool resettable = false;
    int resetValue = 0;
};


