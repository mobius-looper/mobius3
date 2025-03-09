/**
 * For the ValueSets within ParmeterSets, they are each assigned a reference
 * number, also called the "ordinal" in some places.  It differs from most
 * ordinals in that zero means "no selection" so the reference number is always
 * 1+ the location in the list.
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "ValueSet.h"

#include "ParameterSets.h"

ParameterSets::ParameterSets(ParameterSets* src)
{
    for (auto set : src->sets) {
        sets.add(new ValueSet(set));
    }
    ordinate();
}

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
    ordinate();
}

juce::String ParameterSets::toXml()
{
    juce::XmlElement root ("ParameterSets");

    for (auto set : sets)
      set->render(&root);

    return root.toString();
}

juce::OwnedArray<ValueSet>& ParameterSets::getSets()
{
    return sets;
}

ValueSet* ParameterSets::getByOrdinal(int number)
{
    ValueSet* found = nullptr;

    if (number > 0 && number <= sets.size()) {
        found = sets[number - 1];
    }

    // trust but verify
    if (found != nullptr && found->number != number) {
        Trace(1, "ParameterSets: Fixing inconsistent ordinal");
        found->number = number;
    }
    
    return found;
}

ValueSet* ParameterSets::getByIndex(int index)
{
    ValueSet* found = nullptr;
    if (index >= 0 && index < sets.size())
      found = sets[index];
    return found;
}

ValueSet* ParameterSets::find(juce::String name)
{
    ValueSet* found = nullptr;
    int index = 0;

    for (auto set : sets) {
        if (set->name != name)
          index++;
        else {
            found = set;
            break;
        }
    }

    // ordinate should have handled this
    int expected = index + 1;
    if (found != nullptr && found->number != expected) {
        Trace(1, "ParameterSets: Fixing inconsistent ordinal");
        found->number = expected;
    }

    return found;
}

void ParameterSets::ordinate()
{
    int number = 1;
    for (auto set : sets) {
        set->number = number;
        number++;
    }
}

void ParameterSets::add(ValueSet* set)
{
    sets.add(set);
    ordinate();
}

bool ParameterSets::remove(ValueSet* set)
{
    bool removed = false;
    if (sets.contains(set)) {
        sets.removeObject(set, true);
        removed = true;
    }
    return removed;
}

void ParameterSets::clear()
{
    sets.clear();
}

void ParameterSets::transfer(ParameterSets* src)
{
    clear();
    while (src->sets.size() > 0) {
        ValueSet* vs = src->sets.removeAndReturn(0);
        sets.add(vs);
    }
    ordinate();
}

/**
 * todo: The way this is working, it will move the modified set
 * to the end which is semantically okay, but makes the file diffs larger.
 * would be nicer for development to replace it preserving the current location.
 */
void ParameterSets::replace(ValueSet* neu)
{
    if (neu->name.length() == 0) {
        Trace(1, "ParameterSets::replace Refusing set without name");
        delete neu;
    }
    else {
        ValueSet* existing = find(neu->name);
        if (existing != nullptr)
          remove(existing);
        add(neu);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
