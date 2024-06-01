/**
 * Model for actions and parameters that can vary at runtime
 * and not defined with static objects or from the XML files.
 *
 * This has been replaced by the Symbol concept and was gutted.
 * Keep it around for potential future use.
 */

#include <JuceHeader.h>

#include "DynamicConfig.h"

//////////////////////////////////////////////////////////////////////
//
// DynamicConfig
//
//////////////////////////////////////////////////////////////////////

DynamicConfig::DynamicConfig()
{
}

// learn how to use copy constructors you moron
DynamicConfig::DynamicConfig(DynamicConfig* src)
{
    if (src != nullptr) {
        // this has to copy easier, right?
        juce::StringArray* srcErrors = src->getErrors();
        for (int i = 0 ; i < srcErrors->size() ; i++) {
            errors.add((*srcErrors)[i]);
        }
    }
}

DynamicConfig::~DynamicConfig()
{
}

juce::StringArray* DynamicConfig::getErrors()
{
    return &errors;
}

void DynamicConfig::clearErrors()
{
    errors.clear();
}

void DynamicConfig::addError(juce::String error)
{
    errors.add(error);
}

