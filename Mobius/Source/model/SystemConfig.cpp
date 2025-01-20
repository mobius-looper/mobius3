
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
    return values.getJString("startupSession");
}

void SystemConfig::setStartupSession(juce::String name)
{
    values.setJString("startupSession", name);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
