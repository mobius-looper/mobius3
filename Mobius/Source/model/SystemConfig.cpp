
#include <JuceHeader.h>

#include "BindingSets.h"
#include "GroupDefinition.h"

#include "SystemConfig.h"

void SystemConfig::parseXml(juce::XmlElement* root, juce::StringArray& errors)
{
    // you shouldn't be parsing into an already loaded object, but it could happen
    values.clear();
        
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName(ValueSet::XmlElement)) {
            values.parse(el);
        }
        else if (el->hasTagName(BindingSets::XmlName)) {
            bindings.reset(new BindingSets());
            bindings->parseXml(el, errors);
        }
        else if (el->hasTagName(GroupDefinitions::XmlName)) {
            groups.reset(new GroupDefinitions());
            groups->parseXml(el, errors);
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
    
    if (bindings != nullptr)
      bindings->toXml(&root);

    if (groups != nullptr)
      groups->toXml(&root);

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

BindingSets* SystemConfig::getBindings()
{
    return bindings.get();
}

void SystemConfig::setBindings(BindingSets* sets)
{
    bindings.reset(sets);
}

GroupDefinitions* SystemConfig::getGroups()
{
    return groups.get();
}

void SystemConfig::setGroups(GroupDefinitions* newGroups)
{
    groups.reset(newGroups);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
