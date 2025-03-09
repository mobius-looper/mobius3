
#include <JuceHeader.h>

#include "SystemConfig.h"

void SystemConfig::parseXml(juce::XmlElement* root, juce::StringArray& errors)
{
    // you shouldn't be parsing into an already loaded object, but it could happen
    values.clear();
        
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName(ValueSet::XmlElement)) {
            values.parse(el);
        }
        else {
            errors.add(juce::String("SystemConfig: Invalid child element ") + el->getTagName());
        }
    }
}

juce::String SystemConfig::toXml()
{
    juce::XmlElement root (XmlElementName);

    values.render(&root);

    return root.toString();
}

juce::String SystemConfig::getStartupSession()
{
    return values.getJString(StartupSession);
}

void SystemConfig::setStartupSession(juce::String name)
{
    values.setJString(StartupSession, name);
}

ValueSet* SystemConfig::getValues()
{
    return &values;
}

const char* SystemConfig::getString(juce::String name)
{
    return values.getString(name);
}

int SystemConfig::getInt(juce::String name)
{
    return values.getInt(name);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
