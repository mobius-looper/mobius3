
#include <JuceHeader.h>

#include "Binding.h"

Binding::Binding(Binding* src)
{
    copy(src);
}

void Binding::copy(Binding* src)
{
    trigger = src->trigger;
    triggerValue = src->triggerValue;
    midiChannel = src->midiChannel;
    release = src->release;
    symbol = src->symbol;
    arguments = src->arguments;
    scope = src->scope;

    // think these are okay
    uid = src->uid;
    source = src->source;
    displayName = src->displayName;
}

void Binding::parseXml(juce::XmlElement* root, juce::StringArray& errors)
{
    trigger = triggerFromString(root->getStringAttribute("trigger"));
    triggerValue = root->getIntAttribute("value");
    midiChannel = root->getIntAttribute("channel");
    release = root->getBoolAttribute("release");

    symbol = root->getStringAttribute("symbol");
    arguments = root->getStringAttribute("arguments");
    scope = root->getStringAttribute("scope");
    
    for (auto* el : root->getChildIterator()) {
        errors.add(juce::String("BindingSet: Unexpected XML tag name: " + el->getTagName()));
    }
}

void Binding::toXml(juce::XmlElement* parent)
{
    juce::XmlElement* root = new juce::XmlElement(XmlName);

    // looks better to have this first
    if (symbol.length() > 0) root->setAttribute("symbol", symbol);
    
    root->setAttribute("trigger", triggerToString(trigger));
    if (triggerValue > 0) root->setAttribute("value", triggerValue);
    if (midiChannel > 0) root->setAttribute("channel", midiChannel);
    if (release) root->setAttribute("release", release);
    
    if (scope.length() > 0) root->setAttribute("scope", scope);
    if (arguments.length() > 0) root->setAttribute("arguments", arguments);
    
    parent->addChildElement(root);
}

bool Binding::isMidi()
{
	return (trigger == Binding::TriggerNote ||
			trigger == Binding::TriggerProgram ||
			trigger == Binding::TriggerControl);
}
    
Binding::Trigger Binding::triggerFromString(juce::String s)
{
    Trigger t = TriggerUnknown;
    if (s == "note")
      t = TriggerNote;
    else if (s == "control")
      t = TriggerControl;
    else if (s == "program")
      t = TriggerProgram;
    else if (s == "key")
      t = TriggerKey;
    else if (s == "ui")
      t = TriggerUI;
    else if (s == "host")
      t = TriggerHost;
    return t;
}

juce::String Binding::triggerToString(Trigger t)
{
    juce::String s;
    switch (t) {
        case TriggerUnknown: s = "unknown"; break;
        case TriggerNote: s = "note"; break;
        case TriggerControl: s = "control"; break;
        case TriggerProgram: s = "program"; break;
        case TriggerKey: s = "key"; break;
        case TriggerUI: s = "ui"; break;
        case TriggerHost: s = "host"; break;
    }
    return s;
}

