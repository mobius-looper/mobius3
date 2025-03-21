/**
 * For the BindingSets within BindingSets, they are each assigned a reference
 * number, also called the "ordinal" in some places.  It differs from most
 * ordinals in that zero means "no selection" so the reference number is always
 * 1+ the location in the list.
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "old/BindingSet.h"

#include "BindingSets.h"

BindingSets::BindingSets(BindingSets* src)
{
    for (auto set : src->sets) {
        sets.add(new BindingSet(set));
    }
    ordinate();
}

void BindingSets::parseXml(juce::XmlElement* root, juce::StringArray& errors)
{
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName("BindingSet")) {
            BindingSet* set = new BindingSet();
            // todo: make this return errors in the passed array
            set->parse(el);
            sets.add(set);
        }
        else {
            errors.add(juce::String("BindingSets: Unexpected XML tag name: " + el->getTagName()));
        }
    }
    ordinate();
}

juce::String BindingSets::toXml()
{
    juce::XmlElement root (XmlElementName);

    for (auto set : sets)
      set->render(&root);

    return root.toString();
}

juce::OwnedArray<BindingSet>& BindingSets::getSets()
{
    return sets;
}

BindingSet* BindingSets::getByOrdinal(int number)
{
    BindingSet* found = nullptr;

    if (number > 0 && number <= sets.size()) {
        found = sets[number - 1];
    }

    // trust but verify
    if (found != nullptr && found->number != number) {
        Trace(1, "BindingSets: Fixing inconsistent ordinal");
        found->number = number;
    }
    
    return found;
}

BindingSet* BindingSets::getByIndex(int index)
{
    BindingSet* found = nullptr;
    if (index >= 0 && index < sets.size())
      found = sets[index];
    return found;
}

BindingSet* BindingSets::find(juce::String name)
{
    BindingSet* found = nullptr;
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
        Trace(1, "BindingSets: Fixing inconsistent ordinal");
        found->number = expected;
    }

    return found;
}

void BindingSets::ordinate()
{
    int number = 1;
    for (auto set : sets) {
        set->number = number;
        number++;
    }
}

void BindingSets::add(BindingSet* set)
{
    sets.add(set);
    ordinate();
}

bool BindingSets::remove(BindingSet* set)
{
    bool removed = false;
    if (sets.contains(set)) {
        sets.removeObject(set, true);
        removed = true;
    }
    return removed;
}

void BindingSets::clear()
{
    sets.clear();
}

void BindingSets::transfer(BindingSets* src)
{
    clear();
    while (src->sets.size() > 0) {
        BindingSet* vs = src->sets.removeAndReturn(0);
        sets.add(vs);
    }
    ordinate();
}

/**
 * todo: The way this is working, it will move the modified set
 * to the end which is semantically okay, but makes the file diffs larger.
 * would be nicer for development to replace it preserving the current location.
 */
void BindingSets::replace(BindingSet* neu)
{
    if (neu->name.length() == 0) {
        Trace(1, "BindingSets::replace Refusing set without name");
        delete neu;
    }
    else {
        BindingSet* existing = find(neu->name);
        if (existing != nullptr)
          remove(existing);
        add(neu);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
