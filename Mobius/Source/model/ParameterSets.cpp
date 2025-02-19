
#include <JuceHeader.h>

#include "ValueSet.h"

#include "ParameterSets.h"

void ParameterSets::parseXml(juce::XmlElement* root, juce::StringArray& errors)
{
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName("ValueSet")) {
            ValueSet* set = new ValueSet();
            // todo: make this return errors in the passed array
            set->parse(el);
            sets.add(set);
        }
        else {
            errors.add(juce::String("ParameterSets: Unexpected XML tag name: " + el->getTagName()));
        }
    }
}

juce::String ParameterSets::toXml()
{
    juce::XmlElement root ("ParameterSets");

    for (auto set : sets)
      set->render(&root);

    return root.toString();
}

ValueSet* ParameterSets::find(juce::String name)
{
    ValueSet* found = nullptr;

    for (auto set : sets) {
        if (set->name == name) {
            found = set;
            break;
        }
    }
    return found;
}
