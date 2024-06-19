/**
 * Extension of LogPanel to add formatting of MidiMessages
 *
 * Still too closely dependent on MidiDevicesPanel for routing of the events.
 * Events can come from two places, either directly from a MidiInput callback
 * or indirectly through the plugin host with a rather tortured path up from
 * the audio thread.
 *
 * Try to move that junk in here so we can use this outside MidiDevicesPanel
 */

#include <JuceHeader.h>
#include "../Supervisor.h"
#include "../MidiManager.h"
#include "MidiLog.h"

/**
 * MidiManager Listener
 */
void MidiLog::midiMessage(const juce::MidiMessage& message, juce::String& source)
{
    juce::String msg = source + ": ";
    
    int size = message.getRawDataSize();
    const juce::uint8* data = message.getRawData();

    if (message.isNoteOn()) {
        msg += "Note on " +
            juce::String(message.getChannel()) + " " +
            juce::String(message.getNoteNumber()) + " " +
            juce::String(message.getVelocity());
    }
    else if (message.isNoteOff()) {
        msg += "Note off " +
            juce::String(message.getChannel()) + " " +
            juce::String(message.getNoteNumber()) + " " +
            juce::String(message.getVelocity());
    }
    else if (message.isProgramChange()) {
        msg += "Program " +
            juce::String(message.getChannel()) + " " +
            juce::String(message.getProgramChangeNumber());
    }
    else if (message.isController()) {
        msg += "CC " +
            juce::String(message.getChannel()) + " " +
            juce::String(message.getControllerNumber()) + " " +
            juce::String(message.getControllerValue());
    }
    else if (message.isMidiStart()) {
        msg += "Start ";
    }
    else if (message.isMidiStop()) {
        msg += "Stop ";
    }
    else if (message.isMidiContinue()) {
        msg += "Continue ";
    }
    else {
        for (int i = 0 ; i < size ; i++) {
            msg += juce::String(data[i]) + " ";
        }
    }
    
    add(msg);
}

void MidiLog::showOpen()
{
    // show what we have
    MidiManager* mm = Supervisor::Instance->getMidiManager();
    juce::StringArray list = mm->getOpenInputDevices();
    if (list.size() > 0) {
        juce::String msg("Open inputs: " + list.joinIntoString(","));
        add(msg);
    }
    list = mm->getOpenOutputDevices();
    if (list.size() > 0) {
        juce::String msg("Open outputs: " + list.joinIntoString(","));
        add(msg);
    }
    list = mm->getErrors();
    for (auto error : list) {
        add(error);
    }
}
