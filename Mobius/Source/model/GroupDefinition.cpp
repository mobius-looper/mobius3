/**
 * Not much to this yet...
 */

#include <JuceHeader.h>

#include "GroupDefinition.h"

GroupDefinition::GroupDefinition()
{
}

GroupDefinition::GroupDefinition(GroupDefinition* src)
{
    name = src->name;
    color = src->color;
    replicationEnabled = src->replicationEnabled;
    replicatedFunctions = src->replicatedFunctions;
    replicatedParameters = src->replicatedParameters;
}

GroupDefinition::~GroupDefinition()
{
}

//////////////////////////////////////////////////////////////////////
// GroupDefinitions
//////////////////////////////////////////////////////////////////////

GroupDefinitions::GroupDefinitions(GroupDefinitions* src)
{
    for (auto g : src->groups) {
        add(new GroupDefinition(g));
    }
}

void GroupDefinitions::add(GroupDefinition* g)
{
    groups.add(g);
}

int GroupDefinitions::getGroupIndex(juce::String name)
{
    int index = -1;
    for (int i = 0 ; i < groups.size() ; i++) {
        GroupDefinition* def = groups[i];
        if (def->name == name) {
            index = i;
            break;
        }
    }
    return index;
}

GroupDefinition* GroupDefinitions::getGroupByIndex(int index)
{
    GroupDefinition* found = nullptr;
    if (index >= 0 && index < groups.size())
      found = groups[index];
    return found;
}

void GroupDefinitions::getGroupNames(juce::StringArray& names)
{
    for (auto g : groups) {
        names.add(g->name);
    }
}

void GroupDefinitions::toXml(juce::XmlElement* parent)
{
    juce::XmlElement* root = new juce::XmlElement(XmlName);
    parent->addChildElement(root);
    
    for (auto group : groups) {
        
        juce::XmlElement* gel = new juce::XmlElement(GroupDefinition::XmlName);
        root->addChildElement(gel);

        gel->setAttribute("name", group->name);
        if (group->color != 0) gel->setAttribute("color", group->color);
        if (group->replicationEnabled) gel->setAttribute("replicationEnabled", group->replicationEnabled);
        if (group->replicatedFunctions.size() > 0)
          gel->setAttribute("replicatedFunctions", group->replicatedFunctions.joinIntoString(","));
        if (group->replicatedParameters.size() > 0)
          gel->setAttribute("replicatedParameters", group->replicatedParameters.joinIntoString(","));
    }
}

void GroupDefinitions::parseXml(juce::XmlElement* root, juce::StringArray& errors)
{
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName(GroupDefinition::XmlName)) {
            GroupDefinition* def = new GroupDefinition();
            groups.add(def);

            def->name = el->getStringAttribute("name");
            def->color = el->getIntAttribute("color");
            def->replicationEnabled = el->getBoolAttribute("replicationEnabled");

            juce::String csv = el->getStringAttribute("replicatedFunctions");
            def->replicatedFunctions = juce::StringArray::fromTokens(csv, ",", "");
            
            csv = el->getStringAttribute("replicatedParameters");
            def->replicatedParameters = juce::StringArray::fromTokens(csv, ",", "");
        }
        else {
            errors.add(juce::String("GroupDefinitions: Unexpected XML tag name: " +
                                    el->getTagName()));
        }
    }
    
}
