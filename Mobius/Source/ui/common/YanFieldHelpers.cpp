/**
 * Kind of kludgey but generalizing this would be far more complicated and
 * we don't have many of these.
 */

#incldue <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../Provider.h"

#include "YanField.h"
#include "YanFieldHelpers.h"


/**
 * Condition a combo box for a MIDI input device.
 */
void YanFieldHelpers::comboInit(Provider* p, YanCombo* combo, juce::String type, juce::String value)
{
    if (type == "midiInput") {
        initMidiInput(p, combo, value);
    }
    else if (type == "midiOutput") {
        initMidiOutput(p, combo, value);
    }
    else if (type == "trackGroup") {
        initTrackGroup(p, combo, value);
    }
    else {
        Trace(1, "YanFieldHelpers: Unknown helper type %s", type.toUTF8());
    }
}

juce::String YanFieldHelpers::comboSave(YanCombo* combo, juce::String type);
{
    juce::String result;
    if (type == "midiInput") {
        result = saveMidiInput(combo);
    }
    else if (type == "midiOutput") {
        result = saveMidiOutput(combo);
    }
    else if (type == "trackGroup") {
        result = saveTrackGroup(combo);
    }
    else {
        Trace(1, "YanFieldHelpers: Unknown helper type %s", type.toUTF8());
    }
    return result;
}

/////////////////////////////////////////////////////////////////////
// MIDI Input
/////////////////////////////////////////////////////////////////////

void YanFieldHelpers::initMidiInput(Provider* p, YanCombo* combo, juce::String value)
{
    MidiManager* mm = p->getMidiManager();
    juce::StringArray names = mm->getOpenInputDevices();
    if (p->isPlugin())
      names.insert(0, "Host");
    else
      names.insert(0, "Any");
    
    combo->setItems(names);

    int index = 0;
    if (value.length() > 0) {
        index = names.indexOf(value);   
        if (index < 0) {
            // soften the level since this can hit the trace breakpoint
            // every time the window comes up
            // todo: should show something in the editor so they know
            Trace(2, "YanFieldHelper: Warning: Saved MIDI input device not available %s", value);
            index = 0;
        }
    }
    combo->setSelection(index);
}

juce::String YanFieldHelpers::saveMidiInput(YanCombo* combo)
{
    juce::String devname = combo->getSelectionText();
    if (devname == "Any") {
        // don't save this
        devname = "";
    }
    return devname;
}

/////////////////////////////////////////////////////////////////////
// MIDI Output
/////////////////////////////////////////////////////////////////////

void YanFieldHelpers::initMidiOutput(Provider* p, YanCombo* combo, juce::String value)
{
    MidiManager* mm = p->getMidiManager();
    juce::StringArray names = mm->getOpenOutputDevices();
    if (p->isPlugin())
      names.insert(0, "Host");
    // output device defaults to the first one
    combo->setItems(names);
    
    int index = 0;
    if (value.length() > 0) {
        index = names.indexOf(value);   
        if (index < 0) {
            Trace(2, "YanFieldHelper: Warning: Saved MIDI output device not available %s", value);
            index = 0;
        }
    }
    combo->setSelection(index);
}

juce::String YanFieldHelpers::saveMidiOutput(YanCombo* combo)
{
    // nothing special
    return combo->getSelectionText();
}

//////////////////////////////////////////////////////////////////////
// Track Group
//////////////////////////////////////////////////////////////////////

void YanFieldHelpers::initTrackGroup(Provider* p, YanCombo* combo, juce::String value)
{
    MobiusConfig* config = supervisor->getMobiusConfig();
    juce::StringArray names;

    names.add("[None]");
    for (auto def : config->groups)
      names.add(def->name);

    combo->setItems(names);

    int ordinal = config->getGroupOrdinal(value);
    // ordinal is -1 if not found, which matches [None]
    combo->setSelection(ordinal + 1);
}

void YanFieldHelpers::saveTrackGroup(YanCombo* combo)
{
    juce::String result;

    // filter the [None]
    int ordinal = combo->getSelection();
    if (ordinal > 0) {
        result = combo->getSelectionText();
    }
    return result;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

