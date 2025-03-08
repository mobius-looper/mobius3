/**
 * Kind of kludgey but generalizing this would be far more complicated and
 * we don't have many of these.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/ParameterSets.h"
#include "../../model/ValueSet.h"
#include "../../Provider.h"
#include "../../MidiManager.h"
#include "../../Grouper.h"

#include "YanField.h"
#include "YanFieldHelpers.h"

/**
 * Initialize a combo box with values pulled from the system config
 * and return those values.
 */
void YanFieldHelpers::comboInit(Provider* p, YanCombo* combo, juce::String type, juce::StringArray& values)
{
    if (type == "midiInput") {
        initMidiInput(p, combo, values);
    }
    else if (type == "midiOutput") {
        initMidiOutput(p, combo, values);
    }
    else if (type == "trackGroup") {
        initTrackGroup(p, combo, values);
    }
    else if (type == "parameterSet") {
        initParameterSet(p, combo, values);
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

void YanFieldHelpers::initMidiInput(Provider* p, YanCombo* combo, juce::StringArray& values)
{
    MidiManager* mm = p->getMidiManager();
    juce::StringArray names = mm->getOpenInputDevices();

    values.clear();
    if (p->isPlugin())
      values.insert(0, "Host");
    else
      values.insert(0, "Any");
    
    values.addArray(names);
    
    combo->setItems(values);
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

void YanFieldHelpers::initMidiOutput(Provider* p, YanCombo* combo, juce::StringArray& values)
{
    MidiManager* mm = p->getMidiManager();
    juce::StringArray names = mm->getOpenOutputDevices();

    values.clear();
    if (p->isPlugin())
      values.insert(0, "Host");

    values.addArray(names);
    combo->setItems(names);
}

juce::String YanFieldHelpers::saveMidiOutput(YanCombo* combo)
{
    // nothing special
    return combo->getSelectionText();
}

//////////////////////////////////////////////////////////////////////
// Track Group
//////////////////////////////////////////////////////////////////////

void YanFieldHelpers::initTrackGroup(Provider* p, YanCombo* combo, juce::StringArray& values)
{
    Grouper* gp = p->getGrouper();

    values.clear();
    gp->getGroupNames(values);

    values.insert(0, "[None]");
    
    combo->setItems(values);
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
// Parameter Sets
//////////////////////////////////////////////////////////////////////

void YanFieldHelpers::initParameterSet(Provider* p, YanCombo* combo, juce::StringArray& values)
{
    ParameterSets* ps = p->getParameterSets();

    values.clear();
    // assuming we always want a "no selection" option 
    values.add("[None]");
    for (auto set : ps->getSets()) {
        values.add(set->name);
    }
    combo->setItems(values);
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

