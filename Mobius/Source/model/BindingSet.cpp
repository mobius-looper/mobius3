
#include <JuceHeader.h>

#include "Binding.h"
#include "BindingSet.h"

BindingSet::BindingSet(BindingSet* src)
{
    name = src->name;
    overlay = src->overlay;
    
    for (auto b : src->getBindings()) {
        bindings.add(new Binding(b));
    }
}

juce::OwnedArray<Binding>& BindingSet::getBindings()
{
    return bindings;
}

void BindingSet::parseXml(juce::XmlElement* root, juce::StringArray& errors)
{
    name = root->getStringAttribute("name");
    overlay = root->getBoolAttribute("overlay");
    
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName(Binding::XmlName)) {
            Binding* b = new Binding();
            // todo: make this return errors in the passed array
            b->parseXml(el, errors);
            bindings.add(b);
        }
        else {
            errors.add(juce::String("BindingSet: Unexpected XML tag name: " + el->getTagName()));
        }
    }
}

void BindingSet::toXml(juce::XmlElement* parent)
{
    juce::XmlElement* root = new juce::XmlElement(XmlName);
    parent->addChildElement(root);

    if (name.length() > 0) root->setAttribute("name", name);
    if (overlay) root->setAttribute("overlay", "true");
    
    for (auto b : bindings)
      b->toXml(root);
}

void BindingSet::add(Binding* b)
{
    bindings.add(b);
}

void BindingSet::remove(Binding* b)
{
    bindings.removeObject(b, true);
}
