/**
 * Utility class used by Supervisor once during startup to upgrade mobius.xml
 * and other config files for model changes.
 */

#include <JuceHeader.h>

#include "util/List.h"

#include "model/MobiusConfig.h"
#include "model/Session.h"
#include "model/ValueSet.h"
#include "model/Preset.h"
#include "model/Setup.h"
#include "model/Symbol.h"
#include "model/SymbolId.h"
#include "model/FunctionProperties.h"
#include "model/ParameterProperties.h"

#include "Symbolizer.h"
#include "Supervisor.h"

/**
 * Kludge to adjust port numbers which were being incorrectly saved 1 based rather
 * than zero based.  Unfortunately this means imported Setups will have to be imported again.
 *
 * Also does the function properties conversion, and normalizes group names.
 */
bool Upgrader::upgrade(MobiusConfig* config, Session* session)
{
    (void)session;
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
#if 0    
    bool convertValueSets = false;
    if (convertValueSets) {
        if (refreshMainConfig(config, session))
          updated = true;
    }

#endif
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
    int oldGroupCount = config->getTrackGroupsDeprecated();
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
            int groupNumber = track->getGroupNumberDeprecated();
            if (groupNumber > 0) {
                if (track->getGroupName().length() > 0) {
                    // already upgraded, stop using the number
                    // hmm, bindings would rather use ordinals, normalize the there too?
                    track->setGroupNumberDeprecated(0);
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
                    track->setGroupNumberDeprecated(0);
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
#if 0
/**
 * This does partial migration of MoibusConfig into MainConfig so code
 * that needs global variables can start using MainConfig.
 *
 * Actually....stubbing this out until we can address the name constant issues.
 *
 *
 * First obvious thing...we're going to need name constants for all these.
 * They exist in UIParameterIds.h adapt that, but those don't have nice symbolic constants.
 * we have an enum of ids but no fast way to search it.
 *
 */
bool Upgrader::refreshMainConfig(MobiusConfig* old, MainConfig* neu)
{
    SymbolTable* symbols = supervisor->getSymbols();
    
    // wire this on for now
    bool updated = true;

    ValueSet* global = neu->getGlobals();
    global->setInt(symbols->getName(ParamInputLatency), old->getInputLatency());
    global->setInt(symbols->getName(ParamOutputLatency), old->getOutputLatency());
    global->setInt(symbols->getName(ParamNoiseFloor), old->getNoiseFloor());
    global->setInt(symbols->getName(ParamFadeFrames), old->getFadeFrames());
    global->setInt(symbols->getName(ParamMaxSyncDrift), old->getNoiseFloor());
    global->setInt("coreTracks", old->getCoreTracks());
    global->setInt(symbols->getName(ParamMaxLoops), old->getMaxLoops());
    global->setInt(symbols->getName(ParamLongPress), old->getLongPress());
    global->setInt(symbols->getName(ParamSpreadRange), old->getSpreadRange());
    global->setInt(symbols->getName(ParamControllerActionThreshold), old->mControllerActionThreshold);
    global->setBool(symbols->getName(ParamMonitorAudio), old->isMonitorAudio());
    global->setString(symbols->getName(ParamQuickSave), old->getQuickSave());
    global->setString(symbols->getName(ParamActiveSetup), old->getStartingSetupName());
    
    // enumerations have been stored with symbolic names, which is all we really need
    // but I'd like to test working backward to get the ordinals, need to streamline
    // this process
    convertEnum(symbols->getName(ParamDriftCheckPoint), old->getDriftCheckPoint(), global);
    convertEnum(symbols->getName(ParamMidiRecordMode), old->getMidiRecordMode(), global);

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
void Upgrader::convertEnum(juce::String name, int value, ValueSet* dest)
{
    // method 1: using UIParameter static objects
    // this needs to go away
    const char* cname = name.toUTF8();
    UIParameter* p = UIParameter::find(cname);
    const char* enumName = nullptr;
    if (p == nullptr) {
        Trace(1, "Upgrader: Unresolved old parameter %s", cname);
    }
    else {
        enumName = p->getEnumName(value);
        if (enumName == nullptr)
          Trace(1, "Upgrader: Unresolved old enumeration %s %d", cname, value);
    }

    // method 2: using Symbol and ParameterProperties
    Symbol* s = supervisor->getSymbols()->find(name);
    if (s == nullptr) {
        Trace(1, "Upgrader: Unresolved symbol %s", cname);
    }
    else if (s->parameterProperties == nullptr) {
        Trace(1, "Upgrader: Symbol not a parameter %s", cname);
    }
    else {
        const char* newEnumName = s->parameterProperties->getEnumName(value);
        if (newEnumName == nullptr)
          Trace(1, "Upgrader: Unresolved new enumeration %s %d", cname, value);
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

/**
 * These are the rare cases where parameter name constants are necessary.
 * Use GetParameterName
 */
void Upgrader::convertPreset(Preset* preset, MainConfig* main)
{
    SymbolTable* symbols = supervisor->getSymbols();
    
    juce::String objname = juce::String("Preset:") + preset->getName();

    ValueSet* neu = main->find(objname);
    if (neu == nullptr) {
        neu = new ValueSet();
        neu->name = objname;
        main->add(neu);
    }

    neu->setInt(symbols->getName(ParamSubcycles), preset->getSubcycles());
    convertEnum(symbols->getName(ParamMultiplyMode), preset->getMultiplyMode(), neu);
    convertEnum(symbols->getName(ParamShuffleMode), preset->getShuffleMode(), neu);
    neu->setBool(symbols->getName(ParamAltFeedbackEnable), preset->isAltFeedbackEnable());
    convertEnum(symbols->getName(ParamEmptyLoopAction), preset->getEmptyLoopAction(), neu);
    convertEnum(symbols->getName(ParamEmptyTrackAction), preset->getEmptyTrackAction(), neu);
    convertEnum(symbols->getName(ParamTrackLeaveAction), preset->getTrackLeaveAction(), neu);
    neu->setInt(symbols->getName(ParamLoopCount), preset->getLoops());
    convertEnum(symbols->getName(ParamMuteMode), preset->getMuteMode(), neu);
    convertEnum(symbols->getName(ParamMuteCancel), preset->getMuteCancel(), neu);
    neu->setBool(symbols->getName(ParamOverdubQuantized), preset->isOverdubQuantized());
    convertEnum(symbols->getName(ParamQuantize), preset->getQuantize(), neu);
    convertEnum(symbols->getName(ParamBounceQuantize), preset->getBounceQuantize(), neu);
    neu->setBool(symbols->getName(ParamRecordResetsFeedback), preset->isRecordResetsFeedback());
    neu->setBool(symbols->getName(ParamSpeedRecord), preset->isSpeedRecord());
    neu->setBool(symbols->getName(ParamRoundingOverdub), preset->isRoundingOverdub());
    convertEnum(symbols->getName(ParamSwitchLocation), preset->getSwitchLocation(), neu);
    convertEnum(symbols->getName(ParamReturnLocation), preset->getReturnLocation(), neu);
    convertEnum(symbols->getName(ParamSwitchDuration), preset->getSwitchDuration(), neu);
    convertEnum(symbols->getName(ParamSwitchQuantize), preset->getSwitchQuantize(), neu);
    convertEnum(symbols->getName(ParamTimeCopyMode), preset->getTimeCopyMode(), neu);
    convertEnum(symbols->getName(ParamSoundCopyMode), preset->getSoundCopyMode(), neu);
    neu->setInt(symbols->getName(ParamRecordThreshold), preset->getRecordThreshold());
    neu->setBool(symbols->getName(ParamSwitchVelocity), preset->isSwitchVelocity());
    neu->setInt(symbols->getName(ParamMaxUndo), preset->getMaxUndo());
    neu->setInt(symbols->getName(ParamMaxRedo), preset->getMaxRedo());
    neu->setBool(symbols->getName(ParamNoFeedbackUndo), preset->isNoFeedbackUndo());
    neu->setBool(symbols->getName(ParamNoLayerFlattening), preset->isNoLayerFlattening());
    neu->setBool(symbols->getName(ParamSpeedShiftRestart), preset->isSpeedShiftRestart());
    neu->setBool(symbols->getName(ParamPitchShiftRestart), preset->isPitchShiftRestart());
    neu->setInt(symbols->getName(ParamSpeedStepRange), preset->getSpeedStepRange());
    neu->setInt(symbols->getName(ParamSpeedBendRange), preset->getSpeedBendRange());
    neu->setInt(symbols->getName(ParamPitchStepRange), preset->getPitchStepRange());
    neu->setInt(symbols->getName(ParamPitchBendRange), preset->getPitchBendRange());
    neu->setInt(symbols->getName(ParamTimeStretchRange), preset->getTimeStretchRange());
    convertEnum(symbols->getName(ParamSlipMode), preset->getSlipMode(), neu);
    neu->setInt(symbols->getName(ParamSlipTime), preset->getSlipTime());
    neu->setInt(symbols->getName(ParamAutoRecordTempo), preset->getAutoRecordTempo());
    neu->setInt(symbols->getName(ParamAutoRecordBars), preset->getAutoRecordBars());
    convertEnum(symbols->getName(ParamRecordTransfer), preset->getRecordTransfer(), neu);
    convertEnum(symbols->getName(ParamOverdubTransfer), preset->getOverdubTransfer(), neu);
    convertEnum(symbols->getName(ParamReverseTransfer), preset->getReverseTransfer(), neu);
    convertEnum(symbols->getName(ParamSpeedTransfer), preset->getSpeedTransfer(), neu);
    convertEnum(symbols->getName(ParamPitchTransfer), preset->getPitchTransfer(), neu);
    convertEnum(symbols->getName(ParamWindowSlideUnit), preset->getWindowSlideUnit(), neu);
    convertEnum(symbols->getName(ParamWindowEdgeUnit), preset->getWindowEdgeUnit(), neu);
    neu->setInt(symbols->getName(ParamWindowSlideAmount), preset->getWindowSlideAmount());
    neu->setInt(symbols->getName(ParamWindowEdgeAmount), preset->getWindowEdgeAmount());
}

void Upgrader::convertSetup(Setup* setup, MainConfig* main)
{
    SymbolTable* symbols = supervisor->getSymbols();
    
    juce::String objname = juce::String("Setup:") + setup->getName();

    ValueSet* neu = main->find(objname);
    if (neu == nullptr) {
        neu = new ValueSet();
        neu->name = objname;
        main->add(neu);
    }

    neu->setString(symbols->getName(ParamDefaultPreset), setup->getDefaultPresetName());
    convertEnum(symbols->getName(ParamDefaultSyncSource), setup->getSyncSource(), neu);
    convertEnum(symbols->getName(ParamDefaultTrackSyncUnit), setup->getSyncTrackUnit(), neu);
    convertEnum(symbols->getName(ParamSlaveSyncUnit), setup->getSyncUnit(), neu);
    neu->setBool(symbols->getName(ParamManualStart), setup->isManualStart());
    neu->setInt(symbols->getName(ParamMinTempo), setup->getMinTempo());
    neu->setInt(symbols->getName(ParamMaxTempo), setup->getMaxTempo());
    neu->setInt(symbols->getName(ParamBeatsPerBar), setup->getBeatsPerBar());
    convertEnum(symbols->getName(ParamMuteSyncMode), setup->getMuteSyncMode(), neu);
    convertEnum(symbols->getName(ParamResizeSyncAdjust), setup->getResizeSyncAdjust(), neu);
    convertEnum(symbols->getName(ParamSpeedSyncAdjust), setup->getSpeedSyncAdjust(), neu);
    convertEnum(symbols->getName(ParamRealignTime), setup->getRealignTime(), neu);
    convertEnum(symbols->getName(ParamOutRealign), setup->getOutRealignMode(), neu);
    neu->setInt(symbols->getName(ParamActiveTrack), setup->getActiveTrack());

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
    SymbolTable* symbols = supervisor->getSymbols();

    // just to get started, the name of this subset is the track number
    juce::String tname = juce::String(trackNumber);
    
    ValueSet* tset = setup->getSubset(tname);
    if (tset == nullptr) {
        tset = new ValueSet();
        tset->name = tname;
        setup->addSubset(tset);
    }

    tset->setString(symbols->getName(ParamTrackName), track->getName());
    tset->setString(symbols->getName(ParamTrackPreset), track->getTrackPresetName());

    // why is this a parameter?  it's transient
    //tset->setString(ParamActivePreset, track->getActivePreset());

    tset->setBool(symbols->getName(ParamFocus), track->isFocusLock());

    // should have been upgraded to a name by now
    juce::String gname = track->getGroupName();
    if (gname.length() > 0)
      tset->setString(symbols->getName(ParamGroupName), gname.toUTF8());
        
    tset->setBool(symbols->getName(ParamMono), track->isMono());
    tset->setInt(symbols->getName(ParamFeedback), track->getFeedback());
    tset->setInt(symbols->getName(ParamAltFeedback), track->getAltFeedback());
    tset->setInt(symbols->getName(ParamInput), track->getInputLevel());
    tset->setInt(symbols->getName(ParamOutput), track->getOutputLevel());
    tset->setInt(symbols->getName(ParamPan), track->getPan());
    
    convertEnum(symbols->getName(ParamSyncSource), track->getSyncSource(), tset);
    convertEnum(symbols->getName(ParamTrackSyncUnit), track->getSyncTrackUnit(), tset);
        
    tset->setInt(symbols->getName(ParamAudioInputPort), track->getAudioInputPort());
    tset->setInt(symbols->getName(ParamAudioOutputPort), track->getAudioOutputPort());
    tset->setInt(symbols->getName(ParamPluginInputPort), track->getPluginInputPort());
    tset->setInt(symbols->getName(ParamPluginOutputPort), track->getPluginOutputPort());

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
#endif

    
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
