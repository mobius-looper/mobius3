/**
 * This violates encapsulation 8 ways to Sunday, but it's an isolated
 * utility of limited duration.
 */

#include <JuceHeader.h>

#include "../Supervisor.h"
#include "../model/ScriptConfig.h"
#include "../mobius/core/Mobius.h"
#include "../mobius/core/Scriptarian.h"

#include "MslTranslator.h"

/**
 * Sadly, the old parser is file based and can't just parse a String
 */
juce::String MslTranslator::translate(juce::String filename)
{
    juce::File root = Supervsor::Instance->getRoot();
    juce::File file = root.getChildFile(filename);

}    
