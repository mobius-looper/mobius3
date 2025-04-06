
#include <JuceHeader.h>

#include "../../util/MidiUtil.h"
#include "../../model/Binding.h"
#include "../../Binderator.h"
#include "../../KeyTracker.h"

#include "BindingUtil.h"

juce::String BindingUtil::renderTrigger(Binding* b)
{
    juce::String text;
    Binding::Trigger trigger = b->trigger;

    // the menu displays channels as one based, not sure what
    // most people expect
    juce::String channel;
    if (b->midiChannel > 0)
      channel = juce::String(b->midiChannel) + ":";
    
    if (trigger == Binding::TriggerNote) {
        // old utility
        char buf[32];
        MidiNoteName(b->triggerValue, buf);
        text += channel;
        text += buf;
        // not interested in velocity
    }
    else if (trigger == Binding::TriggerProgram) {
        text += channel;
        text += "Pgm ";
        text += juce::String(b->triggerValue);
    }
    else if (trigger == Binding::TriggerControl) {
        text += channel;
        text += "CC ";
        text += juce::String(b->triggerValue);
    }
    else if (trigger == Binding::TriggerKey) {
        // unpack our compressed code/modifiers value
        int code;
        int modifiers;
        Binderator::unpackKeyQualifier(b->triggerValue, &code, &modifiers);
        return KeyTracker::getKeyText(code, modifiers);
    }
    else if (trigger == Binding::TriggerHost) {
        text = "Host";
    }
    else {
        text = "???";
    }
    
    return text;
}

int BindingUtil::unrenderKeyText(juce::String value)
{
    // undo the text transformation that was captured or typed in
    int code = 0;
    int modifiers = 0;
    KeyTracker::parseKeyText(value, &code, &modifiers);

    return Binderator::getKeyQualifier(code, modifiers);
}

/**
 * I think the old way stored these as text and they were
 * parsed at runtime into the mTrack and mGroup numbers
 * Need a lot more here as we refine what scopes mean.
 */ 
juce::String BindingUtil::renderScope(Binding* b)
{
    if (b->scope == "")
      return juce::String("Focused");
    else
      return b->scope;
}

