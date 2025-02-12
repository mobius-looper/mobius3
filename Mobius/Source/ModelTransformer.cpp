
#include <JuceHeader.h>

#include "util/Trace.h"

#include "model/MobiusConfig.h"
#include "model/Setup.h"
#include "model/ParameterConstants.h"

#include "model/Symbol.h"
#include "model/ParameterProperties.h"
#include "model/ValueSet.h"
// why the fuck were using this and not Symbol?
#include "model/UIParameter.h"

#include "model/Session.h"

#include "Provider.h"
#include "ModelTransformer.h"

ModelTransformer::ModelTransformer(Provider* p)
{
    provider = p;
    symbols = p->getSymbols();
}

ModelTransformer::~ModelTransformer()
{
}

void ModelTransformer::addGlobals(MobiusConfig* config, Session* session)
{
    transform(config, session);
}

/**
 * After configing the destination Session to have the right number
 * of audio tracks, copy the parameters from the SetupTracks into those
 * session tracks.
 *
 * The correspondence between a SetupTrack and a Session::Track is
 * loose since tracks don't always have unique identifiers.  The expectation
 * is that this only happens once, and the destination Session will have stubbed
 * out or corrupted tracks and exact correspondence doesn't matter.  We will match them
 * by position rather than name or contents.
 */
void ModelTransformer::merge(Setup* src, Session* dest)
{
    // pull in the name and a very few other things
    transform(src, dest);
    
    int max = dest->getAudioTracks();
    int trackIndex = 0;

    // then a careful merge of the tracks, ignoring extra ones
    for (SetupTrack* t = src->getTracks() ; t != nullptr && trackIndex < max ; t = t->getNext()) {
        Session::Track* destTrack = dest->getTrackByType(Session::TypeAudio, trackIndex);
        if (destTrack == nullptr) {
            // reconcile didn't work
            Trace(1, "ModelTransformer: Session track count mismatch, bad reconciliation");
            break;
        }
        else {
            transform(src, t, destTrack);
        }
        trackIndex++;
    }
}

void ModelTransformer::sessionToConfig(Session* src, MobiusConfig* dest)
{
    // the globals
    transform(src, dest);

    // there will only be one Setup, and it is us
    dest->setSetups(nullptr);
    
    Setup* setup = new Setup();
    transform(src, setup);
    dest->addSetup(setup);
    dest->setStartingSetupName(src->getName().toUTF8());
}

//////////////////////////////////////////////////////////////////////
//
// Value Transformers
//
//////////////////////////////////////////////////////////////////////

void ModelTransformer::transform(SymbolId id, const char* value, ValueSet* dest)
{
    if (value != nullptr) {
        Symbol* s = symbols->getSymbol(id);
        if (s != nullptr)
          dest->setString(s->name, value);
    }
}

const char* ModelTransformer::getString(SymbolId id, ValueSet* src)
{
    const char* result = nullptr;
    Symbol* s = symbols->getSymbol(id);
    if (s != nullptr) {
        MslValue* v = src->get(s->name);
        if (v != nullptr)
          result = v->getString();
    }
    return result;
}

void ModelTransformer::transform(SymbolId id, int value, ValueSet* dest)
{
    Symbol* s = symbols->getSymbol(id);
    if (s != nullptr)
      dest->setInt(s->name, value);
}

int ModelTransformer::getInt(SymbolId id, ValueSet* src)
{
    int result = 0;
    Symbol* s = symbols->getSymbol(id);
    if (s != nullptr) {
        MslValue* v = src->get(s->name);
        if (v != nullptr)
          result = v->getInt();
    }
    return result;
}    

void ModelTransformer::transformBool(SymbolId id, bool value, ValueSet* dest)
{
    Symbol* s = symbols->getSymbol(id);
    if (s != nullptr)
      dest->setBool(s->name, value);
}

bool ModelTransformer::getBool(SymbolId id, ValueSet* src)
{
    bool result = false;
    Symbol* s = symbols->getSymbol(id);
    if (s != nullptr) {
        MslValue* v = src->get(s->name);
        if (v != nullptr)
          result = v->getBool();
    }
    return result;
}

/**
 * Build the MslValue for an enumeration preserving both the symbolic enumeration name
 * and the ordinal.
 *
 * This does some consnstency checking that isn't necessary but I want to detect
 * when there are model inconsistencies between UIParameter and ParameterProperties
 * while both still exist.
 */
void ModelTransformer::transformEnum(SymbolId id, int value, ValueSet* dest)
{
    Symbol* s = symbols->getSymbol(id);
    if (s != nullptr) {
    
        // method 1: using UIParameter static objects
        // this needs to go away
        const char* cname = s->getName();
        UIParameter* p = UIParameter::find(cname);
        const char* enumName = nullptr;
        if (p == nullptr) {
            Trace(1, "ModelTransformer: Unresolved old parameter %s", cname);
        }
        else {
            enumName = p->getEnumName(value);
            if (enumName == nullptr)
              Trace(1, "ModelTransformer: Unresolved old enumeration %s %d", cname, value);
        }

        // method 2: using Symbol and ParameterProperties
        if (s->parameterProperties == nullptr) {
            Trace(1, "ModelTransformer: Symbol not a parameter %s", cname);
        }
        else {
            const char* newEnumName = s->parameterProperties->getEnumName(value);
            if (newEnumName == nullptr)
              Trace(1, "ModelTransformer: Unresolved new enumeration %s %d", cname, value);
            else if (enumName != nullptr && strcmp(enumName, newEnumName) != 0)
              Trace(1, "ModelTransformer: Enum name mismatch %s %s", enumName, newEnumName);

            // so we can limp along, use the new name if the old one didn't match
            if (enumName == nullptr)
              enumName = newEnumName;
        }

        // if we were able to find a name from somewhere, install it
        // otherwise something was traced
        if (enumName != nullptr) {
            MslValue* mv = new MslValue();
            mv->setEnum(enumName, value);
            MslValue* existing = dest->replace(s->name, mv);
            if (existing != nullptr) {
                // first time shouldn't have these, anything interesting to say here?
                delete existing;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Globals
//
//////////////////////////////////////////////////////////////////////

/**
 * Convert MobiusConfig global parameters into Session global parameters.
 *
 * Note that this will duplicate them in EVERY Session, and once there
 * they will not be shared.  For most of them, this feels right.  A small
 * number of them might be REALLY global and should go in SystemConfig.
 *
 * Some of the things you may see in mobius.xml are no longer used,
 * or were already converted to a new model like FunctionProperties
 * or GroupDefinition.
 *
 *    groupCount
 *    groupFocusLock
 *    driftCheckPoint
 *    focusLockFunctions
 *    muteCancelFunctions
 *    confirmationFunctions
 *    altFeedbackDisables
 *
 *    activeSetup
 *    hostRewinds
 *    autoFeedbackReduction
 *    isolateOverdubs
 *    integerWaveFile
 *    tracePrintLevel
 *    traceDebugLevel
 *    saveLayers
 *    dualPluginWindow
 *    maxLayerInfo
 *    maxRedoInfo
 *    noSyncBeatRounding
 *    midiReordMode
 *    edpisms
 */
void ModelTransformer::transform(MobiusConfig* src, Session* dest)
{
    ValueSet* values = dest->ensureGlobals();
    
    // this will have the effect of fleshing out SessionTrack objects
    // if there were not enough SetupTracks in the old file
    dest->reconcileTrackCount(Session::TypeAudio, src->getCoreTracksDontUseThis());

    // these really belong in DeviceConfig
    transform(ParamInputLatency, src->getInputLatency(), values);
    transform(ParamOutputLatency, src->getOutputLatency(), values);

    // these are useful
    transform(ParamNoiseFloor, src->getNoiseFloor(), values);
    transform(ParamLongPress, src->getLongPress(), values);
    transformBool(ParamMonitorAudio, src->isMonitorAudio(), values);

    // should be redesigned
    transform(ParamQuickSave, src->getQuickSave(), values);

    // has to do with how Action values are donverted to speed and pitch
    // adjustments
    transform(ParamSpreadRange, src->getSpreadRange(), values);
    
    // obscure old parameter, potentially useful but hidden
    transform(ParamFadeFrames, src->getFadeFrames(), values);

    // not used by the new sync engine, but still useful
    transform(ParamMaxSyncDrift, src->getMaxSyncDrift(), values);
    
    // this never went anywhere, and is probably broken
    transform(ParamControllerActionThreshold, src->mControllerActionThreshold, values);

    // a few more obscure ones, reconsider the need
    transformBool(ParamAutoFeedbackReduction, src->isAutoFeedbackReduction(), values);

    // an old experiment called "No Layer Flattening"
    transformBool(ParamIsolateOverdubs, src->isIsolateOverdubs(), values);

    // another project option, should be part of the session UI
    transformBool(ParamSaveLayers, src->isSaveLayers(), values);

    // will likely want some options around this, but probably not the same
    //transformEnum(ParamDriftCheckPoint, src->getDriftCheckPoint(), values);
}

/**
 * This goes the other direction, used when passing an editing session down
 * to the core which still needs to see the old model.
 */
void ModelTransformer::transform(Session* src, MobiusConfig* dest)
{
    ValueSet* values = src->ensureGlobals();
    
    dest->setCoreTracks(src->getAudioTracks());

    dest->setInputLatency(getInt(ParamInputLatency, values));
    dest->setOutputLatency(getInt(ParamOutputLatency, values));

    dest->setNoiseFloor(getInt(ParamNoiseFloor, values));
    // this should be handled by TrackManager now
    dest->setLongPress(getInt(ParamLongPress, values));
    // does core do this or Kernel?
    dest->setMonitorAudio(getBool(ParamMonitorAudio, values));
    
    dest->setQuickSave(getString(ParamQuickSave,  values));

    // this shouldn't be necessary, only for bindings
    dest->setSpreadRange(getInt(ParamSpreadRange, values));
    
    // these are obscure and could be hidden
    dest->setFadeFrames(getInt(ParamFadeFrames, values));
    
    // ParamMaxSyncDrift is not used by core now
    // ParamControllerActionThreshold is for Binderator

    // a few more obscure ones, reconsider the need
    dest->setAutoFeedbackReduction(getBool(ParamAutoFeedbackReduction, values));

    // this applies to project saving, it can probably go away once projects
    // are redesigned around the Session and be more of a UI level option
    dest->setIsolateOverdubs(getBool(ParamIsolateOverdubs, values));

    // another project option, should be part of the session UI
    dest->setSaveLayers(getBool(ParamSaveLayers, values));

    // todo: maybe Edpisms
}

//////////////////////////////////////////////////////////////////////
//
// Setup
//
//////////////////////////////////////////////////////////////////////

/**
 * Almost nothing from the model is carried forward.  Most of this has to
 * do with synchronization.
 *
 *   activeTrack
 *   resetRetains
 *   bindings (overlay name)
 *   manualStart
 *   minTempo
 *   maxTempo
 *   beatsPerBar
 *   resizeSyncAdjust
 *   speedSyncAdjust
 *   muteSyncMode
 *   outRealignMode
 * 
 * These had a default that would be applied to all tracks unless overridden
 * in the track.  In the new model, each track must have sync parameters, there
 * is no session-wide default.
 *
 *   defaultSyncSource
 *   slaveSyncUnit (default)
 *
 * The Session has already been given the right number of Session::Track objects
 * which was driven from the MobiusConfig::coreTrackCount number.  Old Setups may
 * have more SetupTracks in them than that, those need to be ignored.
 *
 * Note that this only does the (one) top-level Session parameter, tracks
 * are done by merge()
 */
void ModelTransformer::transform(Setup* src, Session* dest)
{
    ValueSet* values = dest->ensureGlobals();

    dest->setName(juce::String(src->getName()));

    // still used by Track
    transform(ParamDefaultPreset, src->getDefaultPresetName(), values);
}

/**
 * Going the other direction we will have accurate track counts
 * in the Session, so we can rebuild all the SetupTracks too
 */
void ModelTransformer::transform(Session* src, Setup* dest)
{
    ValueSet* values = src->ensureGlobals();

    dest->setName(src->getName().toUTF8());

    // still used by Track
    dest->setDefaultPresetName(getString(ParamDefaultPreset, values));

    SetupTrack* tracks = nullptr;
    SetupTrack* last = nullptr;
    juce::String syncUnit;
    
    for (int i = 0 ; i < src->getTrackCount() ; i++) {
        Session::Track* srctrack = src->getTrackByIndex(i);
        if (srctrack->type == Session::TypeAudio) {

            SetupTrack* desttrack = new SetupTrack();
            if (last == nullptr)
              tracks = desttrack;
            else
              last->setNext(desttrack);
            last = desttrack;
            
            transform(srctrack, desttrack);

            // remember this
            if (syncUnit.length() == 0)
              syncUnit = srctrack->getString("syncUnit");
        }
    }

    dest->setTracks(tracks);

    // syncUnit was duplicated into the Session::Tracks,
    // in the Setup it is shared by all tracks
    
    OldSyncUnit slaveUnit = SYNC_UNIT_BAR;
    if (syncUnit == "beat")
      slaveUnit = SYNC_UNIT_BAR;
    dest->setSyncUnit(slaveUnit);
}

//////////////////////////////////////////////////////////////////////
//
// Track
//
//////////////////////////////////////////////////////////////////////

/**
 * The big complication with SessionTrack is the synchronization parameters.
 * They use old enumerations I don't want to use any more and "out" is now "master".
 *
 * Need to reorganize the symbols.xml file to only have things that make sense
 * in the new model.
 * 
 * The containing Setup is supplied so that values that were defaulted can be
 * given the value from the Setup.
 */
void ModelTransformer::transform(Setup* setup, SetupTrack* src, Session::Track* dest)
{
    ValueSet* values = dest->ensureParameters();

    OldSyncSource defaultSyncSource = setup->getSyncSource();
    SyncTrackUnit defaultTrackSyncUnit = setup->getSyncTrackUnit();

    // this gets a special place outside the ValueSet
    dest->name = juce::String(src->getName());

    // tracks can specify an active preset that overrides the default preset
    // from the Setup
    transform(ParamTrackPreset, src->getTrackPresetName(), values);

    // this used to be an ordinal number but it should have been upgraded long ago
    juce::String gname = src->getGroupName();
    if (gname.length() > 0) {
        // for awhile I used the parameter named "groupName" but I want
        // that to be trackGroup now
        //transform(ParamGroupName, gname.toUTF8(), values);
        values->setJString("trackGroup", gname);
    }

    // not sure I want this to work the same way
    transformBool(ParamFocus, src->isFocusLock(), values);

    // not used, but might want this in the mixer
    transformBool(ParamMono, src->isMono(), values);
        
    transform(ParamInput, src->getInputLevel(), values);
    transform(ParamOutput, src->getOutputLevel(), values);
    transform(ParamFeedback, src->getFeedback(), values);
    transform(ParamAltFeedback, src->getAltFeedback(), values);
    transform(ParamPan, src->getPan(), values);

    transform(ParamAudioInputPort, src->getAudioInputPort(), values);
    transform(ParamAudioOutputPort, src->getAudioOutputPort(), values);
    transform(ParamPluginInputPort, src->getPluginInputPort(), values);
    transform(ParamPluginOutputPort, src->getPluginOutputPort(), values);

    // The sync parameters have changed enumerations
    // Setup uses OldSyncSource and OldTrackSyncUnit and the Session
    // uses SyncSource and TrackSyncUnit
    
    // there is no longer a "default" value here that falls back to a higher level
    OldSyncSource syncSource = src->getSyncSource();
    if (syncSource == SYNC_DEFAULT)
      syncSource = defaultSyncSource;

    // note: though the Session doesn't use the same enumeration, the values are the
    // same except that SYNC_OUT is converted to SYNC_MASTER
    const char* newEnum = nullptr;
    switch (syncSource) {
        case SYNC_NONE: newEnum = "none"; break;
        case SYNC_TRACK: newEnum = "track"; break;
        case SYNC_MIDI: newEnum = "midi"; break;
        case SYNC_HOST: newEnum = "host"; break;
        case SYNC_OUT: newEnum = "master"; break;
        case SYNC_TRANSPORT: newEnum = "transport"; break;
    }
    if (newEnum != nullptr)
      values->setString(juce::String("syncSource"), newEnum);

    // similar conversion the names are the same except there is no Default
    // convert this using the old enum for now
    SyncTrackUnit trackUnit = src->getSyncTrackUnit();
    if (trackUnit == TRACK_UNIT_DEFAULT)
      trackUnit = defaultTrackSyncUnit;

    newEnum = nullptr;
    switch (trackUnit) {
        case TRACK_UNIT_SUBCYCLE: newEnum = "subcycle"; break;
        case TRACK_UNIT_CYCLE: newEnum = "cycle"; break;
        case TRACK_UNIT_LOOP: newEnum = "loop"; break;
    }
    if (newEnum != nullptr)
      values->setString(juce::String("trackSyncUnit"), newEnum);
    else
      Trace(1, "ModelTransformer: Error deriving trackSyncUnit");
    
    // old model had only beat and bar, new model adds loop
    // but you won't see that in the Setup
    newEnum = nullptr;
    OldSyncUnit slaveUnit = setup->getSyncUnit();
    switch (slaveUnit) {
        case SYNC_UNIT_BEAT: newEnum = "beat"; break;
        case SYNC_UNIT_BAR: newEnum = "bar"; break;
    }
    if (newEnum != nullptr)
      values->setString(juce::String("syncUnit"), newEnum);
}

void ModelTransformer::transform(Session::Track* src, SetupTrack* dest)
{
    ValueSet* values = src->ensureParameters();

    dest->setName(src->name.toUTF8());

    // tracks can specify an active preset that overrides the default preset
    // from the Setup
    dest->setTrackPresetName(getString(ParamTrackPreset, values));

    // this used to be an ordinal number but it should have been upgraded long ago
    // this shouldn't be necessary any more, groups are handled by TrackManater
    juce::String gname = values->getJString("trackGroup");
    if (gname.length() > 0)
      dest->setGroupName(gname.toUTF8());

    // not sure I want this to work the same way
    dest->setFocusLock(getBool(ParamFocus, values));

    dest->setMono(getBool(ParamMono, values));
        
    dest->setInputLevel(getInt(ParamInput, values));
    dest->setOutputLevel(getInt(ParamOutput, values));
    dest->setFeedback(getInt(ParamFeedback, values));
    dest->setAltFeedback(getInt(ParamAltFeedback, values));
    dest->setPan(getInt(ParamPan, values));

    dest->setAudioInputPort(getInt(ParamAudioInputPort, values));
    dest->setAudioOutputPort(getInt(ParamAudioOutputPort, values));
    dest->setPluginInputPort(getInt(ParamPluginInputPort, values));
    dest->setPluginOutputPort(getInt(ParamPluginOutputPort, values));

    // The sync parameters have changed enumerations
    // Setup uses OldSyncSource and OldTrackSyncUnit and the Session
    // uses SyncSource and TrackSyncUnit

    // sync parameters should not be necessary, but there may be some funny
    // things with them
    juce::String syncSource = values->getString("syncSource");
    OldSyncSource oldSyncSource = SYNC_NONE;
    if (syncSource == "track")
      oldSyncSource = SYNC_TRACK;
    else if (syncSource == "midi")
      oldSyncSource = SYNC_MIDI;
    else if (syncSource == "host")
      oldSyncSource = SYNC_HOST;
    else if (syncSource == "master")
      oldSyncSource = SYNC_OUT;
    else if (syncSource == "transport")
      oldSyncSource = SYNC_TRANSPORT;

    dest->setSyncSource(oldSyncSource);
    
    juce::String trackSyncUnit = values->getString("trackSyncUnit");
    SyncTrackUnit oldUnit = TRACK_UNIT_LOOP;
    if (trackSyncUnit == "subcycle")
      oldUnit = TRACK_UNIT_SUBCYCLE;
    else if (trackSyncUnit == "cycle")
      oldUnit = TRACK_UNIT_CYCLE;
    dest->setSyncTrackUnit(oldUnit);
}

//////////////////////////////////////////////////////////////////////
//
// Pending
//
// Some old prototype code for transforming Presets that will be
// needed someday
//
//////////////////////////////////////////////////////////////////////

#if 0

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
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
