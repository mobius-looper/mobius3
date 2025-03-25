
#include <JuceHeader.h>

#include "BindingSets.h"
#include "GroupDefinition.h"
#include "SampleConfig.h"

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
        else if (el->hasTagName(SampleConfig::XmlName)) {
            samples.reset(new SampleConfig());
            samples->parseXml(el, errors);
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

    if (samples != nullptr)
      samples->toXml(&root);

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

bool SystemConfig::hasBindings()
{
    // used by the Upgrader to see if there are any bindings without
    // bootstrapping an empty container
    return (bindings != nullptr);
}

BindingSets* SystemConfig::getBindings()
{
    // it's convenient not to have to make callers test for nullptr
    // which almost never happens, bootstrap an empty one in this
    // fringe case
    if (bindings == nullptr)
      bindings.reset(new BindingSets());
    
    return bindings.get();
}

void SystemConfig::setBindings(BindingSets* sets)
{
    bindings.reset(sets);
}

GroupDefinitions* SystemConfig::getGroups()
{
    // should do the same thing here as we do for BindingSets
    return groups.get();
}

void SystemConfig::setGroups(GroupDefinitions* newGroups)
{
    groups.reset(newGroups);
}

SampleConfig* SystemConfig::getSamples()
{
    return samples.get();
}

void SystemConfig::setSamples(SampleConfig* newSamples)
{
    samples.reset(newSamples);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
