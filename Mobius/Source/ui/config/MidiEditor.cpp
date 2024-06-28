
#include <JuceHeader.h>

#include <string>
#include <sstream>

#include "../../Supervisor.h"
#include "../../util/Trace.h"
#include "../../util/MidiUtil.h"
#include "../common/Form.h"
#include "../../model/Binding.h"

#include "MidiEditor.h"

MidiEditor::MidiEditor()
{
    setName("MidiEditor");
    initForm();
}

MidiEditor::~MidiEditor()
{
    // remove lingering listener from MidiTracker
    MidiManager* mm = context->getSupervisor()->getMidiManager();
    mm->removeMonitor(this);
}

void MidiEditor::prepare()
{
    context->enableObjectSelector();
}

/**
 * Called by ConfigEditor when we're about to be made visible.
 * So we can support MIDI capture, register as a listener for
 * MIDI events.
 *
 * The Listener style presents a problem here because while we're
 * visible and doing capture, Binderator is also a listener and is
 * happily processing the current bindings, which can be surprising
 * if you're trying to change bindings and the old ones start firing.
 *
 * KeyboardPanel added the notion of an "exclusive" listener to prevent
 * this, give MidiManager one too.
 */
void MidiEditor::showing()
{
    MidiManager* mm = context->getSupervisor()->getMidiManager();
    mm->addMonitor(this);
}

/**
 * Called by ConfigEditor when we're about to be made invisible.
 */
void MidiEditor::hiding()
{
    MidiManager* mm = context->getSupervisor()->getMidiManager();
    mm->removeMonitor(this);
}

/**
 * Called by BindingEditor as it iterates over all the bindings
 * stored in a BindingSet.  Return true if this is for MIDI.
 */
bool MidiEditor::isRelevant(Binding* b)
{
    // TriggerMidi is defined for some reason
    // but I don't think that can be seen in saved bindings
    return (b->trigger == TriggerNote ||
            b->trigger == TriggerProgram ||
            b->trigger == TriggerControl);

    // don't support these yet, and probably won't
    // b->trigger == TriggerPitch);
}

/**
 * Return the string to show in the trigger column for a binding.
 * The Binding has a key code but we want to show a nice symbolic name.
 *
 * Channel zero means : any
 * Specific channels are 1-16
 */
juce::String MidiEditor::renderSubclassTrigger(Binding* b)
{
    juce::String text;
    Trigger* trigger = b->trigger;

    // the menu displays channels as one based, not sure what
    // most people expect
    juce::String channel;
    if (b->midiChannel > 0)
      channel = juce::String(b->midiChannel) + ":";
    
    if (trigger == TriggerNote) {
        // old utility
        char buf[32];
        MidiNoteName(b->triggerValue, buf);
        text += channel;
        text += buf;
        // not interested in velocity
    }
    else if (trigger == TriggerProgram) {
        text += channel;
        text += "Pgm ";
        text += juce::String(b->triggerValue);
    }
    else if (trigger == TriggerControl) {
        text += channel;
        text += "CC ";
        text += juce::String(b->triggerValue);
    }
    else if (trigger == TriggerPitch) {
        // did anyone really use this?
        text += channel;
        text += "Pitch ";
        text += juce::String(b->triggerValue);
    }
    return text;
}

/**
 * Overload of a BindingEditor virtual to insert our fields in between
 * scope and arguments.  Messy control flow and has constructor issues
 * with initForm.  Would be cleaner to give Form a way to insert into
 * existing Forms.
 */
void MidiEditor::addSubclassFields()
{
    messageType = new Field("Type", Field::Type::String);
    juce::StringArray typeNames;
    // could have an array of Triggers for these
    typeNames.add("Note");
    typeNames.add("Control");
    typeNames.add("Program");
    // typeNames.add("Pitch");
    messageType->setAllowedValues(typeNames);
    form.add(messageType);

    // Binding number is the combo index where
    // zero means "any"
    messageChannel = new Field("Channel", Field::Type::String);
    juce::StringArray channelNames;
    channelNames.add("Any");
    for (int i = 1 ; i <= 16 ; i++) {
        channelNames.add(juce::String(i));
    }
    messageChannel->setAllowedValues(channelNames);
    form.add(messageChannel);

    // todo: need to make field smarter about text fields that
    // can only contain digits
    messageValue = new Field("Value", Field::Type::String);
    messageValue->setWidthUnits(4);
    form.add(messageValue);
    
    capture = new Field("MIDI Capture", Field::Type::Boolean);
    form.add(capture);
}

/**
 * Refresh the form fields to show the selected binding
 *
 * todo: Now that we allow the "Any" channel would be nice
 * to have a checkbox to ignore the incomming channel rather
 * than making them set it back to Any after every capture.
 */
void MidiEditor::refreshSubclassFields(class Binding* b)
{
    Trigger* trigger = b->trigger;
    if (trigger == TriggerNote) {
        messageType->setValue(0);
    }
    else if (trigger == TriggerControl) {
        messageType->setValue(1);
    }
    else if (trigger == TriggerProgram) {
        messageType->setValue(2);
    }
    else if (trigger == TriggerPitch) {
        messageType->setValue(3);
    }
    else {
        // shouldn't be here, go back to Note
        messageType->setValue(0);
    }

    messageChannel->setValue(b->midiChannel);
    messageValue->setValue(juce::String(b->triggerValue));
}

/**
 * Put the value of the form fields into the Binding
 */
void MidiEditor::captureSubclassFields(class Binding* b)
{
    int index = messageType->getIntValue();
    switch (index) {
        case 0: b->trigger = TriggerNote; break;
        case 1: b->trigger = TriggerControl; break;
        case 2: b->trigger = TriggerProgram; break;
        case 3: b->trigger = TriggerPitch; break;
    }

    b->midiChannel = messageChannel->getIntValue();
    b->triggerValue = messageValue->getIntValue();
}

void MidiEditor::resetSubclassFields()
{
    messageType->setValue(0);
    messageChannel->setValue(0);
    messageValue->setValue(juce::String());
}

void MidiEditor::midiMonitor(const juce::MidiMessage& message, juce::String& source)
{
    (void)source;
    if (capture->getBoolValue()) {
        int value = -1;
        if (message.isNoteOn()) {
            messageType->setValue(0);
            value = message.getNoteNumber();
        }
        else if (message.isController()) {
            messageType->setValue(1);
            value = message.getControllerNumber();
        }
        else if (message.isProgramChange()) {
            messageType->setValue(2);
            value = message.getProgramChangeNumber();
        }
#if 0        
        else if (message.isPitchWheel()) {
            messageType->setValue(3);
            // value is a 14-bit number, and is not significant
            // since there is only one pitch wheel
            value = 0;

        }
#endif        
        if (value >= 0) {
            // channels are 1 based in Juce, 0 if sysex
            // Binding 0 means "any"
            // would be nice to have a checkbox to ignore the channel
            // if they want "any"
            int ch = message.getChannel();
            if (ch > 0)
              messageChannel->setValue(ch);
            messageValue->setValue(value);
        }
    }
}

bool MidiEditor::midiMonitorExclusive()
{
    return true;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


