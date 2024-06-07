/**
 * A form panel for editing presets.
 */

#include <JuceHeader.h>

#include "../../model/MobiusConfig.h"
#include "../../model/UIParameter.h"
#include "../../model/XmlRenderer.h"

#include "../common/Form.h"
#include "../JuceUtil.h"

#include "ParameterField.h"
#include "ConfigEditor.h"
#include "PresetPanel.h"

PresetPanel::PresetPanel(ConfigEditor* argEditor) :
    ConfigPanel{argEditor, "Presets",
      ConfigPanelButton::Save | ConfigPanelButton::Revert | ConfigPanelButton::Cancel, true}
{
    // debugging hack
    setName("PresetPanel");
    render();
}

PresetPanel::~PresetPanel()
{
    // members will delete themselves
}

//////////////////////////////////////////////////////////////////////
//
// ConfigPanel overloads
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by ConfigEditor when asked to edit presets.
 * 
 * If this is the first time being shown, copy the Preset list from the
 * master configuration and load the first preset.
 *
 * If we are already active, just display what we currently have.
 * ConfigEditor will handle visibility.
 */
void PresetPanel::load()
{
    if (!loaded) {
        // build a list of names for the object selector
        juce::Array<juce::String> names;
        // clone the Preset list into a local copy
        presets.clear();
        revertPresets.clear();
        MobiusConfig* config = editor->getMobiusConfig();
        if (config != nullptr) {
            // convert the linked list to an OwnedArray
            Preset* plist = config->getPresets();
            while (plist != nullptr) {
                Preset* p = new Preset(plist);
                presets.add(p);
                names.add(juce::String(plist->getName()));
                // also a copy for the revert list
                revertPresets.add(new Preset(plist));

                plist = (Preset*)(plist->getNext());
            }
        }
        
        // this will also auto-select the first one
        objectSelector.setObjectNames(names);

        // load the first one, do we need to bootstrap one if
        // we had an empty config?
        selectedPreset = 0;
        loadPreset(selectedPreset);

        loaded = true;
        // force this true for testing
        changed = true;
    }
}

/**
 * Refresh the object selector on initial load and after any
 * objects are added or removed.
 */
void PresetPanel::refreshObjectSelector()
{
    juce::Array<juce::String> names;
    for (auto preset : presets) {
        if (preset->getName() == nullptr)
          preset->setName("[New]");
        names.add(preset->getName());
    }
    objectSelector.setObjectNames(names);
    objectSelector.setSelectedObject(selectedPreset);
}

/**
 * Called by the Save button in the footer.
 * 
 * Save all presets that have been edited during this session
 * back to the master configuration.
 *
 * Tell the ConfigEditor we are done.
 */
void PresetPanel::save()
{
    if (changed) {
        // copy visible state back into the Preset
        // need to also do this when the selected preset is changed
        savePreset(selectedPreset);
        
        // build a new Preset linked list
        Preset* plist = nullptr;
        Preset* last = nullptr;
        
        for (int i = 0 ; i < presets.size() ; i++) {
            Preset* p = presets[i];
            if (last == nullptr)
              plist = p;
            else
              last->setNext(p);
            last = p;
        }

        // we took ownership of the objects so
        // clear the owned array but don't delete them
        presets.clear(false);

        MobiusConfig* config = editor->getMobiusConfig();
        // this will also delete the current preset list
        config->setPresets(plist);
        editor->saveMobiusConfig();

        loaded = false;
        changed = false;
    }
    else if (loaded) {
        // throw away preset copies
        presets.clear();
        loaded = false;
    }
}

/**
 * Throw away all editing state.
 */
void PresetPanel::cancel()
{
    // delete the copied presets
    presets.clear();
    revertPresets.clear();
    loaded = false;
    changed = false;
}

//////////////////////////////////////////////////////////////////////
//
// ObjectSelector overloads
// 
//////////////////////////////////////////////////////////////////////

/**
 * Called when the combobox changes.
 */
void PresetPanel::selectObject(int ordinal)
{
    if (ordinal != selectedPreset) {
        savePreset(selectedPreset);
        selectedPreset = ordinal;
        loadPreset(selectedPreset);
    }
}

void PresetPanel::newObject()
{
    int newOrdinal = presets.size();
    Preset* neu = new Preset();
    neu->setName("[New]");

    // copy the current preset into the new one, I think
    // this is more convenient than initializing it, could have an init button?
    // or a copy button distinct from new
    //neu->copy(presets[selectedPreset]);

    presets.add(neu);
    // make another copy for revert
    Preset* revert = new Preset(neu);
    revertPresets.add(revert);

    selectedPreset = newOrdinal;
    loadPreset(selectedPreset);
    
    objectSelector.addObjectName(juce::String(neu->getName()));
    objectSelector.setSelectedObject(newOrdinal);
}

/**
 * Delete is somewhat complicated.
 * You can't undo it unless we save it somewhere.
 * An alert would be nice, ConfigPanel could do that.
 */
void PresetPanel:: deleteObject()
{
    if (presets.size() == 1) {
        // must have at least one preset
    }
    else {
        presets.remove(selectedPreset);
        revertPresets.remove(selectedPreset);
        // leave the index where it was and show the next one,
        // if we were at the end, move back
        int newOrdinal = selectedPreset;
        if (newOrdinal >= presets.size())
          newOrdinal = presets.size() - 1;
        selectedPreset = newOrdinal;
        loadPreset(selectedPreset);
    }
}

void PresetPanel::revertObject()
{
    Preset* revert = revertPresets[selectedPreset];
    if (revert != nullptr) {
        Preset* reverted = new Preset(revert);
        presets.set(selectedPreset, reverted);
        // what about the name?
        loadPreset(selectedPreset);
    }
}

void PresetPanel::renameObject(juce::String newName)
{
    Preset* preset = presets[selectedPreset];
    preset->setName(objectSelector.getObjectName().toUTF8());
}

//////////////////////////////////////////////////////////////////////
//
// Internal Methods
// 
//////////////////////////////////////////////////////////////////////

/**
 * Load a preset into the parameter fields
 */
void PresetPanel::loadPreset(int index)
{
    Preset* p = presets[index];
    if (p != nullptr) {
        juce::Array<Field*> fields;
        form.gatherFields(fields);
        for (int i = 0 ; i < fields.size() ; i++) {
            Field* f = fields[i];
            ParameterField* pf = dynamic_cast<ParameterField*>(f);
            if (pf != nullptr) {
              pf->loadValue(p);
            }
        }
    }
}

/**
 * Save one of the edited presets back to the master config
 * Think...should save/cancel apply to the entire list of presets
 * or only the one currently being edited.  I think it would be confusing
 * to keep an editing transaction over the entire list
 * When a preset is selected, it should throw away changes that
 * are in progress to the current preset.
 */
void PresetPanel::savePreset(int index)
{
    Preset* p = presets[index];
    if (p != nullptr) {
        juce::Array<Field*> fields;
        form.gatherFields(fields);
        for (int i = 0 ; i < fields.size() ; i++) {
            Field* f = fields[i];
            ParameterField* pf = dynamic_cast<ParameterField*>(f);
            if (pf != nullptr)
              pf->saveValue(p);
        }
    }
}

// who called this?
Preset* PresetPanel::getSelectedPreset()
{
    Preset* p = nullptr;
    if (presets.size() > 0) {
        if (selectedPreset < 0 || selectedPreset >= presets.size()) {
            // shouldn't happen, default back to first
            selectedPreset = 0;
        }
        p = presets[selectedPreset];
    }
    return p;
}

//////////////////////////////////////////////////////////////////////
//
// Form Rendering
//
//////////////////////////////////////////////////////////////////////

void PresetPanel::render()
{
    initForm();
    form.render();

    // place it in the content panel
    content.addAndMakeVisible(form);

    // at this point the component hierarhcy has been fully constructed
    // but not sized, until we support bottom up sizing start with
    // a fixed size, this will cascade resized() down the child hierarchy

    // until we get auto-sizing worked out, make this plenty wide
    // MainComponent is currently 1000x1000
    setSize (900, 600);
}

void PresetPanel::initForm()
{
    form.setHelpArea(&helpArea);

    // this overrides the maxLoops global parameter and
    // belongs in the Setup if we need it at all
    // actually global maxLoops isn't used at all by core code
    // so just use this until we can redesign this
    // loop count should really go in Setup or SetupTrack
    addField("General", UIParameterLoopCount);
    
    addField("General", UIParameterSubcycles);
    addField("General", UIParameterMaxUndo);
    addField("General", UIParameterMaxRedo);
    addField("General", UIParameterNoFeedbackUndo);
    addField("General", UIParameterAltFeedbackEnable);

    addField("Quantize", UIParameterQuantize);
    addField("Quantize", UIParameterSwitchQuantize);
    addField("Quantize", UIParameterBounceQuantize);
    addField("Quantize", UIParameterOverdubQuantized);

    // Record

    addField("Record", UIParameterRecordThreshold);
    addField("Record", UIParameterAutoRecordBars);
    addField("Record", UIParameterAutoRecordTempo);
    addField("Record", UIParameterSpeedRecord);
    addField("Record", UIParameterRecordResetsFeedback);

    // Switch
    addField("Switch", UIParameterEmptyLoopAction);
    addField("Switch", UIParameterEmptyTrackAction);
    addField("Switch", UIParameterTrackLeaveAction);
    addField("Switch", UIParameterTimeCopyMode);
    addField("Switch", UIParameterSoundCopyMode);
    addField("Switch", UIParameterSwitchLocation);
    addField("Switch", UIParameterSwitchDuration);
    addField("Switch", UIParameterReturnLocation);
    addField("Switch", UIParameterSwitchVelocity);
    // column 2
    addField("Switch", UIParameterRecordTransfer, 1);
    addField("Switch", UIParameterOverdubTransfer, 1);
    addField("Switch", UIParameterReverseTransfer, 1);
    addField("Switch", UIParameterSpeedTransfer, 1);
    addField("Switch", UIParameterPitchTransfer, 1);
        
    // Functions
    addField("Functions", UIParameterMultiplyMode);
    addField("Functions", UIParameterShuffleMode);
    addField("Functions", UIParameterMuteMode);
    addField("Functions", UIParameterMuteCancel);
    addField("Functions", UIParameterSlipMode);
    addField("Functions", UIParameterSlipTime);
    addField("Functions", UIParameterWindowSlideUnit);
    addField("Functions", UIParameterWindowSlideAmount);
    addField("Functions", UIParameterWindowEdgeUnit);
    addField("Functions", UIParameterWindowEdgeAmount);
    // column 2
    addField("Functions", UIParameterRoundingOverdub, 1);
        
    // Effects
    //addField("Effects", UIParameterSpeedSequence);
    //addField("Effects", UIParameterPitchSequence);
    addField("Effects", UIParameterSpeedShiftRestart);
    addField("Effects", UIParameterPitchShiftRestart);
    addField("Effects", UIParameterSpeedStepRange);
    addField("Effects", UIParameterSpeedBendRange);
    addField("Effects", UIParameterPitchStepRange);
    addField("Effects", UIParameterPitchBendRange);
    addField("Effects", UIParameterTimeStretchRange);

    // Sustain
    // this was never used, keep it out until we need it
    // I see Sustainable in various places around Action
    // handling, maybe we replaced it with that?
    //form.add("Sustain", SustainFunctionsParameter);
}

void PresetPanel::addField(const char* tab, UIParameter* p, int col)
{
    form.add(new ParameterField(p), tab, col);
}


/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
