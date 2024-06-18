
#include <JuceHeader.h>

#include <string>
#include <sstream>

#include "../../model/MobiusConfig.h"
#include "../../model/UIParameter.h"

#include "../common/Form.h"
#include "ParameterField.h"
#include "ConfigEditor.h"

#include "GlobalPanel.h"


GlobalPanel::GlobalPanel(ConfigEditor* argEditor) :
    ConfigPanel{argEditor, "Global Parameters", ConfigPanelButton::Save | ConfigPanelButton::Cancel, false}
{
    // debugging hack
    setName("GlobalPanel");
    render();
}

GlobalPanel::~GlobalPanel()
{
}

/**
 * Simpler than Presets and Setups because we don't have multiple objects to deal with.
 * Load fields from the master config at the start, then commit them directly back
 * to the master config.
 */
void GlobalPanel::load()
{
    if (!loaded) {
        MobiusConfig* config = editor->getMobiusConfig();

        loadGlobal(config);

        // force this true for testing
        changed = true;
    }
}

void GlobalPanel::save()
{
    if (changed) {
        MobiusConfig* config = editor->getMobiusConfig();
        saveGlobal(config);
        editor->saveMobiusConfig();
        changed = false;
    }
}

void GlobalPanel::cancel()
{
    changed = false;
}

/**
 * Load the global confif into the parameter fields
 */
void GlobalPanel::loadGlobal(MobiusConfig* config)
{
    juce::Array<Field*> fields;
    form.gatherFields(fields);
    for (int i = 0 ; i < fields.size() ; i++) {
        Field* f = fields[i];
        ParameterField* pf = dynamic_cast<ParameterField*>(f);
        if (pf != nullptr)
          pf->loadValue(config);
    }
}

/**
 * Save the fields back into the master config.
 */
void GlobalPanel::saveGlobal(MobiusConfig* config)
{
    juce::Array<Field*> fields;
    form.gatherFields(fields);
    for (int i = 0 ; i < fields.size() ; i++) {
        Field* f = fields[i];
        ParameterField* pf = dynamic_cast<ParameterField*>(f);
        if (pf != nullptr)
          pf->saveValue(config);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Rendering
//
//////////////////////////////////////////////////////////////////////

void GlobalPanel::render()
{
    initForm();
    form.render();

    // place it in the content panel
    content.addAndMakeVisible(form);

    // we can either auto size at this point or try to
    // make all config panels a uniform size
    setSize (900, 600);
}

/**
 * These parameters show in the old dialog but are no longer needed.
 *  Custom Message File, CustomMessageFileParameter
 *  Message Duration/MessageDurationParameter
 *   this is actually a UIType, not in MobiusConfig
 *  Dual Plugin Edit Window
 *
 * These are in MobiusConfig but are edited in dedicated panels
 *   AudioInputParameter
 *   AudioOutputParameter
 *   MidiInputParameter
 *   MidiOutputParameter
 *
 * See parameter-shit for others that are defined but obscure.
 * 
 */
void GlobalPanel::initForm()
{
    // would be nice to have HelpArea more built-in to ConfigPanel
    // but it doesn't know about the Form which is where
    // the linkage starts, so have to duplicate this in every subclass
    form.setHelpArea(&helpArea);
    
    // note that while activeSetup is a global parameter we defined
    // it in the SetupPanel as a side effect of leaving it selected?
    // do we?
    // what about DefaultPreset shouldn't that be the same?
    //addField("Miscellaneous", UIParameterDefaultPreset);

    // These are the most useful
    addField("General", UIParameterTrackCount);
    addField("General", UIParameterGroupCount);

    // this doesn't actually do anything in core code
    // loops-per-track has been set in the Preset which
    // isn't ideal
    //addField("General", UIParameterMaxLoops);
    
    addField("General", UIParameterPluginPorts);
    addField("General", UIParameterQuickSave);
    addField("General", UIParameterLongPress);
    addField("General", UIParameterAutoFeedbackReduction);

    // these are obscure
    addField("Advanced", UIParameterInputLatency);
    addField("Advanced", UIParameterOutputLatency);
    addField("Advanced", UIParameterMaxSyncDrift);
    addField("Advanced", UIParameterNoiseFloor);
    addField("Advanced", UIParameterTraceLevel);
    addField("Advanced", UIParameterSaveLayers);
    addField("Advanced", UIParameterMonitorAudio);

    // support lost, could be useful
    //addField("Advanced", UIParameterSpreadRange);
    
    // support for this got lost
    // might be useful to resurrect
    //addField("General", UIParameterIntegerWaveFile);

    // this one is obscure and either needs to be removed
    // or implemented properly, new Actionator doesn't handle it
    //addField("Miscellaneous", UIParameterGroupFocusLock);

    // these exist as booleans in the model, but I'm doing export a different way now
    //addField("Miscellaneous", UIParameterMidiExport);
    //addField("Miscellaneous", UIParameterHostMidiExport);

    // these are StringList and need rework
    //addField("Functions", UIParameterFocusLockFunctions);
    //addField("Functions", UIParameterMuteCancelFunctions);
    //addField("Functions", UIParameterConfirmationFunctions);
    // addField("Modes", UIParameterAltFeedbackDisable);
}

void GlobalPanel::addField(const char* tab, UIParameter* p)
{
    form.add(new ParameterField(p), tab, 0);
}


/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

