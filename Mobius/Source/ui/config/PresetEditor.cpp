/**
 * A ConfigEditor for editing Presets.
 */

#include <JuceHeader.h>

#include "../../model/MobiusConfig.h"
#include "../../model/UIParameter.h"
#include "../../Supervisor.h"

#include "../common/Form.h"
#include "../JuceUtil.h"

#include "ParameterField.h"
#include "PresetEditor.h"

PresetEditor::PresetEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("PresetEditor");
}

PresetEditor::~PresetEditor()
{
    // members will delete themselves
}

void PresetEditor::prepare()
{
    context->enableObjectSelector();
    context->enableHelp(40);
    context->enableRevert();

    initForm();
    form.render();
    addAndMakeVisible(form);
}

void PresetEditor::resized()
{
    form.setBounds(getLocalBounds());
}

/**
 * Load all the Presets, nice and fresh.
 */
void PresetEditor::load()
{
    // build a list of names for the object selector
    juce::Array<juce::String> names;
    // clone the Preset list into a local copy
    presets.clear();
    revertPresets.clear();
    MobiusConfig* config = supervisor->getOldMobiusConfig();
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
    context->setObjectNames(names);

    // load the first one, do we need to bootstrap one if
    // we had an empty config?
    selectedPreset = 0;
    loadPreset(selectedPreset);
}

/**
 * Refresh the object selector on initial load and after any
 * objects are added or removed.
 */
void PresetEditor::refreshObjectSelector()
{
    juce::Array<juce::String> names;
    for (auto preset : presets) {
        if (preset->getName() == nullptr)
          preset->setName("[New]");
        names.add(preset->getName());
    }
    context->setObjectNames(names);
    context->setSelectedObject(selectedPreset);
}

/**
 * Called by the Save button in footer.
 * 
 * Save all presets that have been edited during this session
 * back to the master configuration.
 */
void PresetEditor::save()
{
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
    revertPresets.clear();

    supervisor->presetEditorSave(plist);
}

/**
 * Throw away all editing state.
 */
void PresetEditor::cancel()
{
    // delete the copied presets
    presets.clear();
    revertPresets.clear();
}

void PresetEditor::revert()
{
    Preset* revert = revertPresets[selectedPreset];
    if (revert != nullptr) {
        Preset* reverted = new Preset(revert);
        presets.set(selectedPreset, reverted);
        // what about the name?
        loadPreset(selectedPreset);
    }
}

//////////////////////////////////////////////////////////////////////
//
// ObjectSelector overloads
// 
//////////////////////////////////////////////////////////////////////

/**
 * Called when the combobox changes.
 */
void PresetEditor::objectSelectorSelect(int ordinal)
{
    if (ordinal != selectedPreset) {
        savePreset(selectedPreset);
        selectedPreset = ordinal;
        loadPreset(selectedPreset);
    }
}

void PresetEditor::objectSelectorNew(juce::String newName)
{
    int newOrdinal = presets.size();
    Preset* neu = new Preset();

    // have not been expecting a new name to be passed, who wins?
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
    
    context->addObjectName(juce::String(neu->getName()));
    context->setSelectedObject(newOrdinal);
}

/**
 * Delete is somewhat complicated.
 * You can't undo it unless we save it somewhere.
 * An alert would be nice, ConfigPanel could do that.
 */
void PresetEditor::objectSelectorDelete()
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
        refreshObjectSelector();
    }
}


void PresetEditor::objectSelectorRename(juce::String newName)
{
    Preset* preset = presets[selectedPreset];
    preset->setName(newName.toUTF8());
}

//////////////////////////////////////////////////////////////////////
//
// Internal Methods
// 
//////////////////////////////////////////////////////////////////////

/**
 * Load a preset into the parameter fields
 */
void PresetEditor::loadPreset(int index)
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
void PresetEditor::savePreset(int index)
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
Preset* PresetEditor::getSelectedPreset()
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

void PresetEditor::initForm()
{
    form.setHelpArea(context->getHelpArea());

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

void PresetEditor::addField(const char* tab, UIParameter* p, int col)
{
    form.add(new ParameterField(supervisor, p), tab, col);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
