
#include <JuceHeader.h>

#include <string>
#include <sstream>

#include "../../util/Trace.h"
#include "../../util/MidiUtil.h"
#include "../../model/old/OldBinding.h"
#include "../../model/UIConfig.h"

#include "../../Supervisor.h"

#include "../common/YanField.h"
#include "../common/YanForm.h"

#include "MidiEditor.h"

MidiEditor::MidiEditor(Supervisor* s) : OldBindingEditor(s)
{
    setName("MidiEditor");
    initForm();
}

MidiEditor::~MidiEditor()
{
    // remove lingering listener from MidiTracker
    MidiManager* mm = supervisor->getMidiManager();
    mm->removeMonitor(this);
}

void MidiEditor::prepare()
{
    context->enableObjectSelector();

    UIConfig* config = supervisor->getUIConfig();
    setInitialObject(config->activeBindings);
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
    MidiManager* mm = supervisor->getMidiManager();
    mm->addMonitor(this);
}

/**
 * Called by ConfigEditor when we're about to be made invisible.
 */
void MidiEditor::hiding()
{
    MidiManager* mm = supervisor->getMidiManager();
    mm->removeMonitor(this);
}

/**
 * Called by BindingEditor as it iterates over all the bindings
 * stored in a BindingSet.  Return true if this is for MIDI.
 */
bool MidiEditor::isRelevant(OldBinding* b)
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
juce::String MidiEditor::renderSubclassTrigger(OldBinding* b)
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
#if 0    
    messageType = new Field("Type", Field::Type::String);
    juce::StringArray typeNames;
    // could have an array of Triggers for these
    typeNames.add("Note");
    typeNames.add("Control");
    typeNames.add("Program");
    // typeNames.add("Pitch");
    messageType->setAllowedValues(typeNames);
    messageType->addListener(this);
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
    messageChannel->addListener(this);
    form.add(messageChannel);

    // todo: need to make field smarter about text fields that
    // can only contain digits
    messageValue = new Field("Value", Field::Type::String);
    messageValue->setWidthUnits(4);
    messageValue->addListener(this);
    form.add(messageValue);
#endif

    juce::StringArray typeNames;
    // could have an array of Triggers for these
    typeNames.add("Note");
    typeNames.add("Control");
    typeNames.add("Program");
    // typeNames.add("Pitch");
    messageType.setItems(typeNames);
    messageType.setListener(this);
    form.add(&messageType);
    // stick a release selector next to it
    addRelease();

    // Binding number is the combo index where
    // zero means "any"
    juce::StringArray channelNames;
    channelNames.add("Any");
    for (int i = 1 ; i <= 16 ; i++) {
        channelNames.add(juce::String(i));
    }
    messageChannel.setItems(channelNames);
    messageChannel.setListener(this);
    form.add(&messageChannel);

    // todo: need to make field smarter about text fields that
    // can only contain digits
    messageValue.setListener(this);
    form.add(&messageValue);
}

/**
 * Refresh the form fields to show the selected binding
 *
 * todo: Now that we allow the "Any" channel would be nice
 * to have a checkbox to ignore the incomming channel rather
 * than making them set it back to Any after every capture.
 */
void MidiEditor::refreshSubclassFields(class OldBinding* b)
{
    Trigger* trigger = b->trigger;
    if (trigger == TriggerNote) {
        messageType.setSelection(0);
    }
    else if (trigger == TriggerControl) {
        messageType.setSelection(1);
    }
    else if (trigger == TriggerProgram) {
        messageType.setSelection(2);
    }
    else if (trigger == TriggerPitch) {
        messageType.setSelection(3);
    }
    else {
        // shouldn't be here, go back to Note
        messageType.setSelection(0);
    }

    messageChannel.setSelection(b->midiChannel);
    messageValue.setValue(juce::String(b->triggerValue));
}

/**
 * Put the value of the form fields into the Binding
 */
void MidiEditor::captureSubclassFields(class OldBinding* b)
{
    int index = messageType.getSelection();
    switch (index) {
        case 0: b->trigger = TriggerNote; break;
        case 1: b->trigger = TriggerControl; break;
        case 2: b->trigger = TriggerProgram; break;
        case 3: b->trigger = TriggerPitch; break;
    }

    b->midiChannel = messageChannel.getSelection();
    b->triggerValue = messageValue.getInt();
}

void MidiEditor::resetSubclassFields()
{
    messageType.setSelection(0);
    messageChannel.setSelection(0);
    messageValue.setValue(juce::String());
}

void MidiEditor::midiMonitor(const juce::MidiMessage& message, juce::String& source)
{
    (void)source;

    bool relevant = message.isNoteOn() || message.isController() || message.isProgramChange();

    if (relevant) {
        if (isCapturing()) {
            int value = -1;
            if (message.isNoteOn()) {
                messageType.setSelection(0);
                value = message.getNoteNumber();
            }
            else if (message.isController()) {
                messageType.setSelection(1);
                value = message.getControllerNumber();
            }
            else if (message.isProgramChange()) {
                messageType.setSelection(2);
                value = message.getProgramChangeNumber();
            }
#if 0        
            else if (message.isPitchWheel()) {
                messageType.setSelection(3);
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
                  messageChannel.setSelection(ch);
                messageValue.setValue(juce::String(value));
            }
        }

        // whether we're capturing or not, tell BindingEditor about this
        // so it can display what is being captured when capture is off
        // sigh, need the equivalent of renderSubclassTrigger but we don't
        // have a binding
        juce::String cap = renderCapture(message);
        showCapture(cap);
    }
}

/**
 * Variant of renderTrigger used for capture.
 * Could share this with a little effort and ensure the formats
 * are consistent.
 */
juce::String MidiEditor::renderCapture(const juce::MidiMessage& msg)
{
    juce::String text;

    // the menu displays channels as one based, not sure what
    // most people expect
    juce::String channel;
    if (msg.getChannel() > 0)
      channel = juce::String(msg.getChannel()) + ":";
    
    if (msg.isNoteOn()) {
        // old utility
        char buf[32];
        MidiNoteName(msg.getNoteNumber(), buf);
        text += channel;
        text += buf;
        // not interested in velocity
    }
    else if (msg.isProgramChange()) {
        text += channel;
        text += "Pgm ";
        text += juce::String(msg.getProgramChangeNumber());
    }
    else if (msg.isController()) {
        text += channel;
        text += "CC ";
        text += juce::String(msg.getControllerNumber());
    }
    return text;
}

bool MidiEditor::midiMonitorExclusive()
{
    return !isCapturePassthrough();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


