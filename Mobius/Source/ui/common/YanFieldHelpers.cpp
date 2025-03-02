/**
 * Kind of kludgey but generalizing this would be far more complicated and
 * we don't have many of these.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Preset.h"
#include "../../model/ParameterSets.h"
#include "../../model/ValueSet.h"
#include "../../Provider.h"
#include "../../MidiManager.h"
#include "../../Grouper.h"

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
    else if (type == "trackPreset") {
        initTrackPreset(p, combo, value);
    }
    else if (type == "parameterSet") {
        initParameterSet(p, combo, value);
    }
    else {
        Trace(1, "YanFieldHelpers: Unknown helper type %s", type.toUTF8());
    }
}

juce::String YanFieldHelpers::comboSave(YanCombo* combo, juce::String type)
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
    else if (type == "trackPreset") {
        result = saveTrackPreset(combo);
    }
    else if (type == "parameterSet") {
        result = saveParameterSet(combo);
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
            Trace(2, "YanFieldHelper: Warning: Saved MIDI input device not available %s",
                  value.toUTF8());
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
            Trace(2, "YanFieldHelper: Warning: Saved MIDI output device not available %s",
                  value.toUTF8());
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
    Grouper* gp = p->getGrouper();

    juce::StringArray names;
    gp->getGroupNames(names);

    names.insert(0, "[None]");
    
    combo->setItems(names);

    int ordinal = gp->getGroupOrdinal(value);
    // ordinal is -1 if not found, which matches [None]
    combo->setSelection(ordinal + 1);
}

juce::String YanFieldHelpers::saveTrackGroup(YanCombo* combo)
{
    juce::String result;

    // filter the [None]
    int ordinal = combo->getSelection();
    if (ordinal > 0) {
        result = combo->getSelectionText();
    }
    return result;
}

//////////////////////////////////////////////////////////////////////
// Track Preset
//////////////////////////////////////////////////////////////////////

void YanFieldHelpers::initTrackPreset(Provider* p, YanCombo* combo, juce::String value)
{
    MobiusConfig* config = p->getOldMobiusConfig();
    
    juce::StringArray names;
    // assuming we always want a "no selection" option 
    names.add("[None]");
    for (Preset* preset = config->getPresets() ; preset != nullptr ;
         preset = preset->getNextPreset()) {
        names.add(juce::String(preset->getName()));
    }
    combo->setItems(names);

    int ordinal = Structure::getOrdinal(config->getPresets(), value.toUTF8());
    // ordinal is -1 if not found, which matches [None]
    combo->setSelection(ordinal + 1);
}

juce::String YanFieldHelpers::saveTrackPreset(YanCombo* combo)
{
    juce::String result;

    // filter the [None]
    int ordinal = combo->getSelection();
    if (ordinal > 0) {
        result = combo->getSelectionText();
    }
    return result;
}

//////////////////////////////////////////////////////////////////////
// Parameter Sets
//////////////////////////////////////////////////////////////////////

void YanFieldHelpers::initParameterSet(Provider* p, YanCombo* combo, juce::String value)
{
    ParameterSets* ps = p->getParameterSets();
    
    juce::StringArray names;
    // assuming we always want a "no selection" option 
    names.add("[None]");
    for (auto set : ps->getSets()) {
        names.add(set->name);
    }
    combo->setItems(names);

    int index = 0;
    if (value.length() > 0) {
        index = names.indexOf(value);   
        if (index < 0) {
            Trace(2, "YanFieldHelper: Warning: Saved ParameterSet not available %s",
                  value.toUTF8());
            index = 0;
        }
    }
    
    combo->setSelection(index);
}

juce::String YanFieldHelpers::saveParameterSet(YanCombo* combo)
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

