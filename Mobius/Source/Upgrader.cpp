/**
 * Utility class used by Supervisor once during startup to upgrade mobius.xml
 * and other config files for model changes.
 */

#include <JuceHeader.h>

#include "util/List.h"

#include "model/MobiusConfig.h"
#include "model/MainConfig.h"
#include "model/ValueSet.h"
#include "model/UIParameterIds.h"
#include "model/Preset.h"
#include "model/Setup.h"
#include "model/Symbol.h"
#include "model/FunctionProperties.h"
#include "model/ParameterProperties.h"

#include "Supervisor.h"

/**
 * Kludge to adjust port numbers which were being incorrectly saved 1 based rather
 * than zero based.  Unfortunately this means imported Setups will have to be imported again.
 *
 * Also does the function properties conversion, and normalizes group names.
 */
bool Upgrader::upgrade(MobiusConfig* config, MainConfig* main)
{
    bool updated = false;
    
    if (config->getVersion() < 1) {
        for (Setup* s = config->getSetups() ; s != nullptr ; s = s->getNextSetup()) {
            // todo: only do this for the ones we know weren't upgraded?
            for (SetupTrack* t = s->getTracks() ; t != nullptr ; t = t->getNext()) {
                t->setAudioInputPort(upgradePort(t->getAudioInputPort()));
                t->setAudioOutputPort(upgradePort(t->getAudioOutputPort()));
                t->setPluginInputPort(upgradePort(t->getPluginInputPort()));
                t->setPluginOutputPort(upgradePort(t->getPluginOutputPort()));
            }
        }
        config->setVersion(1);
        updated = true;
    }

    if (upgradeFunctionProperties(config))
      updated = true;
    
    if (upgradeGroups(config))
      updated = true;

    // not active yet, but start testing the conversion
    if (refreshMainConfig(config, main))
      updated = true;

    return updated;
}

int Upgrader::upgradePort(int number)
{
    // if it's already zero then it has either been upgraded or it hasn't passed
    // through the UI yet
    if (number > 0)
      number--;
    return number;
}

/**
 * Convert the old function name lists into Symbol properties.
 */
bool Upgrader::upgradeFunctionProperties(MobiusConfig* config)
{
    bool updated = false;

    StringList* list = config->getFocusLockFunctions();
    if (list != nullptr && list->size() > 0) {
        upgradeFunctionProperty(list, true, false, false);
        // don't do this again
        config->setFocusLockFunctions(nullptr);
        updated = true;
    }

    list = config->getConfirmationFunctions();
    if (list != nullptr && list->size() > 0) {
        upgradeFunctionProperty(list, false, true, false);
        config->setConfirmationFunctions(nullptr);
        updated = true;
    }
    
    list = config->getMuteCancelFunctions();
    if (list != nullptr && list->size() > 0) {
        upgradeFunctionProperty(list, false, false, true);
        config->setMuteCancelFunctions(nullptr);
        updated = true;
    }

    // todo: there is one name list parameter property: resetRetains
    // could handle that here too, but these were less common and it's actually
    // a Setup parameter so there could be more than one, make the user think about
    // and set the new properties manually

    return updated;
}

void Upgrader::upgradeFunctionProperty(StringList* names, bool focus, bool confirm, bool muteCancel)
{
    if (names != nullptr) {
        for (int i = 0 ; i < names->size() ; i++) {
            const char* name = names->getString(i);
            Symbol* s = supervisor->getSymbols()->find(name);
            if (s == nullptr) {
                Trace(1, "Upgrader::upgradeFunctionProperties Undefined function %s\n", name);
            }
            else if (s->functionProperties == nullptr) {
                // symbols should have been loaded by now, don't bootstrap
                Trace(1, "Upgrader::upgradeFunctionProperties Missing function properties for %s\n", name);
            }
            else {
                if (focus) s->functionProperties->focus = true;
                if (confirm) s->functionProperties->confirmation = true;
                if (muteCancel) s->functionProperties->muteCancel = true;
            }
        }
    }
}

/**
 * Normalize GroupDefinitions and group name references.
 */
bool Upgrader::upgradeGroups(MobiusConfig* config)
{
    bool updated = false;
    
    // add names for prototype definitions that didn't have them
    int ordinal = 0;
    for (auto group : config->groups) {
        if (group->name.length() == 0) {
            group->name = GroupDefinition::getInternalName(ordinal);
            updated = true;
        }
        ordinal++;
    }
    
    // the original group definitions by number
    // make sure we have a GroupDefinition object for all the numbers
    int oldGroupCount = config->getTrackGroups();
    // make sure we have at least 2 for some old expectations
    if (oldGroupCount == 0)
      oldGroupCount = 2;
    
    if (oldGroupCount > config->groups.size()) {
        ordinal = config->groups.size();
        while (ordinal < oldGroupCount) {
            GroupDefinition* neu = new GroupDefinition();
            neu->name = GroupDefinition::getInternalName(ordinal);
            config->groups.add(neu);
            updated = true;
            ordinal++;
        }
    }

    // setups used to reference groups by ordinal
    Setup* setup = config->getSetups();
    while (setup != nullptr) {
        SetupTrack* track = setup->getTracks();
        while (track != nullptr) {
            int groupNumber = track->getGroupNumber();
            if (groupNumber > 0) {
                if (track->getGroupName().length() > 0) {
                    // already upgraded, stop using the number
                    // hmm, bindings would rather use ordinals, normalize the there too?
                    track->setGroupNumber(0);
                }
                else {
                    // this was an ordinal starting from 1
                    int groupIndex = groupNumber - 1;
                    if (groupIndex >= 0 && groupIndex < config->groups.size()) {
                        GroupDefinition* def = config->groups[groupIndex];
                        track->setGroupName(def->name);
                    }
                    else {
                        // here we could treat these like the old maxGroups count
                        // and synthesize new ones to match
                        Trace(1, "Upgrader::upgradeGroups Setup group reference out of range %d",
                              groupNumber);
                    }
                    // stop using the number
                    track->setGroupNumber(0);
                }
                updated = true;
            }
            track = track->getNext();
        }
        setup = setup->getNextSetup();
    }

    return updated;
}

//////////////////////////////////////////////////////////////////////
//
// MainConfig Migration
//
//////////////////////////////////////////////////////////////////////

/**
 * This is test code for the migration from MobiusConfig to MainConfig and
 * parameter sets.
 *
 * It only does model conversion right now so I can watch it, the MainConfig is
 * not yet being used by anything.
 *
 * First obvious thing...we're going to need name constants for all these.
 * They exist in UIParameterIds.h adapt that, but those don't have nice symbolic constants.
 * we have an enum of ids but no fast way to search it.
 *
 */
bool Upgrader::refreshMainConfig(MobiusConfig* old, MainConfig* neu)
{
    // wire this on for now
    bool updated = true;

    ValueSet* global = neu->getGlobals();
    global->setInt(ParamInputLatency, old->getInputLatency());
    global->setInt(ParamOutputLatency, old->getOutputLatency());
    global->setInt(ParamNoiseFloor, old->getNoiseFloor());
    global->setInt(ParamFadeFrames, old->getFadeFrames());
    global->setInt(ParamMaxSyncDrift, old->getNoiseFloor());
    global->setInt(ParamTrackCount, old->getTracks());
    global->setInt(ParamMaxLoops, old->getMaxLoops());
    global->setInt(ParamLongPress, old->getLongPress());
    global->setInt(ParamSpreadRange, old->getSpreadRange());
    global->setInt(ParamControllerActionThreshold, old->mControllerActionThreshold);
    global->setBool(ParamMonitorAudio, old->isMonitorAudio());
    global->setString(ParamQuickSave, old->getQuickSave());
    global->setString(ParamActiveSetup, old->getStartingSetupName());
    
    // enumerations have been stored with symbolic names, which is all we really need
    // but I'd like to test working backward to get the ordinals, need to streamline
    // this process
    convertEnum(ParamDriftCheckPoint, old->getDriftCheckPoint(), global);
    convertEnum(ParamMidiRecordMode, old->getMidiRecordMode(), global);

    Preset* preset = old->getPresets();
    while (preset != nullptr) {
        convertPreset(preset, neu);
        preset = preset->getNextPreset();
    }

    Setup* setup = old->getSetups();
    while (setup != nullptr) {
        convertSetup(setup, neu);
        setup = setup->getNextSetup();
    }
    
    return updated;
}

/**
 * Build the MslValue for an enumeration from this parameter name and ordinal
 * and isntall it in the value set.
 *
 * This does some consnstency checking that isn't necessary but I want to detect
 * when there are model inconsistencies while both still exist.
 */
void Upgrader::convertEnum(const char* name, int value, ValueSet* dest)
{
    // method 1: using UIParameter static objects
    // this needs to go away
    UIParameter* p = UIParameter::find(name);
    const char* enumName = nullptr;
    if (p == nullptr) {
        Trace(1, "Upgrader: Unresolved old parameter %s", name);
    }
    else {
        enumName = p->getEnumName(value);
        if (enumName == nullptr)
          Trace(1, "Upgrader: Unresolved old enumeration %s %d", name, value);
    }

    // method 2: using Symbol and ParameterProperties
    Symbol* s = supervisor->getSymbols()->find(name);
    if (s == nullptr) {
        Trace(1, "Upgrader: Unresolved symbol %s", name);
    }
    else if (s->parameterProperties == nullptr) {
        Trace(1, "Upgrader: Symbol not a parameter %s", name);
    }
    else {
        const char* newEnumName = s->parameterProperties->getEnumName(value);
        if (newEnumName == nullptr)
          Trace(1, "Upgrader: Unresolved new enumeration %s %d", name, value);
        else if (enumName != nullptr && strcmp(enumName, newEnumName) != 0)
          Trace(1, "Upgrader: Enum name mismatch %s %s", enumName, newEnumName);

        // so we can limp along, use the new name if the old one didn't match
        if (enumName == nullptr)
          enumName = newEnumName;
    }

    // if we were able to find a name from somewhere, install it
    // otherwise something was traced
    if (enumName != nullptr) {
        MslValue* mv = new MslValue();
        mv->setEnum(enumName, value);
        MslValue* existing = dest->replace(name, mv);
        if (existing != nullptr) {
            // first time shouldn't have these, anything interesting to say here?
            delete existing;
        }
    }
}

void Upgrader::convertPreset(Preset* preset, MainConfig* main)
{
    juce::String objname = juce::String("Preset:") + preset->getName();

    ValueSet* neu = main->find(objname);
    if (neu == nullptr) {
        neu = new ValueSet();
        neu->name = objname;
        main->add(neu);
    }

    neu->setInt(ParamSubcycles, preset->getSubcycles());
    convertEnum(ParamMultiplyMode, preset->getMultiplyMode(), neu);
    convertEnum(ParamShuffleMode, preset->getShuffleMode(), neu);
    neu->setBool(ParamAltFeedbackEnable, preset->isAltFeedbackEnable());
    convertEnum(ParamEmptyLoopAction, preset->getEmptyLoopAction(), neu);
    convertEnum(ParamEmptyTrackAction, preset->getEmptyTrackAction(), neu);
    convertEnum(ParamTrackLeaveAction, preset->getTrackLeaveAction(), neu);
    neu->setInt(ParamLoopCount, preset->getLoops());
    convertEnum(ParamMuteMode, preset->getMuteMode(), neu);
    convertEnum(ParamMuteCancel, preset->getMuteCancel(), neu);
    neu->setBool(ParamOverdubQuantized, preset->isOverdubQuantized());
    convertEnum(ParamQuantize, preset->getQuantize(), neu);
    convertEnum(ParamBounceQuantize, preset->getBounceQuantize(), neu);
    neu->setBool(ParamRecordResetsFeedback, preset->isRecordResetsFeedback());
    neu->setBool(ParamSpeedRecord, preset->isSpeedRecord());
    neu->setBool(ParamRoundingOverdub, preset->isRoundingOverdub());
    convertEnum(ParamSwitchLocation, preset->getSwitchLocation(), neu);
    convertEnum(ParamReturnLocation, preset->getReturnLocation(), neu);
    convertEnum(ParamSwitchDuration, preset->getSwitchDuration(), neu);
    convertEnum(ParamSwitchQuantize, preset->getSwitchQuantize(), neu);
    convertEnum(ParamTimeCopyMode, preset->getTimeCopyMode(), neu);
    convertEnum(ParamSoundCopyMode, preset->getSoundCopyMode(), neu);
    neu->setInt(ParamRecordThreshold, preset->getRecordThreshold());
    neu->setBool(ParamSwitchVelocity, preset->isSwitchVelocity());
    neu->setInt(ParamMaxUndo, preset->getMaxUndo());
    neu->setInt(ParamMaxRedo, preset->getMaxRedo());
    neu->setBool(ParamNoFeedbackUndo, preset->isNoFeedbackUndo());
    neu->setBool(ParamNoLayerFlattening, preset->isNoLayerFlattening());
    neu->setBool(ParamSpeedShiftRestart, preset->isSpeedShiftRestart());
    neu->setBool(ParamPitchShiftRestart, preset->isPitchShiftRestart());
    neu->setInt(ParamSpeedStepRange, preset->getSpeedStepRange());
    neu->setInt(ParamSpeedBendRange, preset->getSpeedBendRange());
    neu->setInt(ParamPitchStepRange, preset->getPitchStepRange());
    neu->setInt(ParamPitchBendRange, preset->getPitchBendRange());
    neu->setInt(ParamTimeStretchRange, preset->getTimeStretchRange());
    convertEnum(ParamSlipMode, preset->getSlipMode(), neu);
    neu->setInt(ParamSlipTime, preset->getSlipTime());
    neu->setInt(ParamAutoRecordTempo, preset->getAutoRecordTempo());
    neu->setInt(ParamAutoRecordBars, preset->getAutoRecordBars());
    convertEnum(ParamRecordTransfer, preset->getRecordTransfer(), neu);
    convertEnum(ParamOverdubTransfer, preset->getOverdubTransfer(), neu);
    convertEnum(ParamReverseTransfer, preset->getReverseTransfer(), neu);
    convertEnum(ParamSpeedTransfer, preset->getSpeedTransfer(), neu);
    convertEnum(ParamPitchTransfer, preset->getPitchTransfer(), neu);
    convertEnum(ParamWindowSlideUnit, preset->getWindowSlideUnit(), neu);
    convertEnum(ParamWindowEdgeUnit, preset->getWindowEdgeUnit(), neu);
    neu->setInt(ParamWindowSlideAmount, preset->getWindowSlideAmount());
    neu->setInt(ParamWindowEdgeAmount, preset->getWindowEdgeAmount());
}

void Upgrader::convertSetup(Setup* setup, MainConfig* main)
{
    juce::String objname = juce::String("Setup:") + setup->getName();

    ValueSet* neu = main->find(objname);
    if (neu == nullptr) {
        neu = new ValueSet();
        neu->name = objname;
        main->add(neu);
    }

    neu->setString(ParamDefaultPreset, setup->getDefaultPresetName());
    convertEnum(ParamDefaultSyncSource, setup->getSyncSource(), neu);
    convertEnum(ParamDefaultTrackSyncUnit, setup->getSyncTrackUnit(), neu);
    convertEnum(ParamSlaveSyncUnit, setup->getSyncUnit(), neu);
    neu->setBool(ParamManualStart, setup->isManualStart());
    neu->setInt(ParamMinTempo, setup->getMinTempo());
    neu->setInt(ParamMaxTempo, setup->getMaxTempo());
    neu->setInt(ParamBeatsPerBar, setup->getBeatsPerBar());
    convertEnum(ParamMuteSyncMode, setup->getMuteSyncMode(), neu);
    convertEnum(ParamResizeSyncAdjust, setup->getResizeSyncAdjust(), neu);
    convertEnum(ParamSpeedSyncAdjust, setup->getSpeedSyncAdjust(), neu);
    convertEnum(ParamRealignTime, setup->getRealignTime(), neu);
    convertEnum(ParamOutRealign, setup->getOutRealignMode(), neu);
    neu->setInt(ParamActiveTrack, setup->getActiveTrack());

    SetupTrack* track = setup->getTracks();
    int trackNumber = 1;
    while (track != nullptr) {
        convertSetupTrack(track, trackNumber, neu);
        trackNumber++;
        track = track->getNext();
    }
}

void Upgrader::convertSetupTrack(SetupTrack* track, int trackNumber, ValueSet* setup)
{
    ValueSet* tset = setup->getSubset(trackNumber);
    if (tset == nullptr) {
        tset = new ValueSet();
        tset->scope = trackNumber;
        setup->addSubset(tset, trackNumber);
    }

    tset->setString(ParamTrackName, track->getName());
    tset->setString(ParamTrackPreset, track->getTrackPresetName());

    // why is this a parameter?  it's transient
    //tset->setString(ParamActivePreset, track->getActivePreset());

    tset->setBool(ParamFocus, track->isFocusLock());

    // should have been upgraded to a name by now
    juce::String gname = track->getGroupName();
    if (gname.length() > 0)
      tset->setString(ParamGroup, gname.toUTF8());
        
    tset->setBool(ParamMono, track->isMono());
    tset->setInt(ParamFeedback, track->getFeedback());
    tset->setInt(ParamAltFeedback, track->getAltFeedback());
    tset->setInt(ParamInput, track->getInputLevel());
    tset->setInt(ParamOutput, track->getOutputLevel());
    tset->setInt(ParamPan, track->getPan());
    
    convertEnum(ParamSyncSource, track->getSyncSource(), tset);
    convertEnum(ParamTrackSyncUnit, track->getSyncTrackUnit(), tset);
        
    tset->setInt(ParamAudioInputPort, track->getAudioInputPort());
    tset->setInt(ParamAudioOutputPort, track->getAudioOutputPort());
    tset->setInt(ParamPluginInputPort, track->getPluginInputPort());
    tset->setInt(ParamPluginOutputPort, track->getPluginOutputPort());

    // these are defined as parameters but haven't been in the XML for some reason
    /*
    ParamSpeedOctave,
    ParamSpeedStep,
    ParamSpeedBend,
    ParamPitchOctave,
    ParamPitchStep,
    ParamPitchBend,
    ParamTimeStretch
    */
}    

    
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
