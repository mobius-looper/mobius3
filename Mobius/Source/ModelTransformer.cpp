
#include <JuceHeader.h>

#include "util/Trace.h"

#include "model/old/MobiusConfig.h"
#include "model/old/Setup.h"
#include "model/old/Preset.h"
#include "model/ParameterConstants.h"

#include "model/Symbol.h"
#include "model/ParameterProperties.h"
#include "model/ValueSet.h"

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
 */
void ModelTransformer::transformEnum(SymbolId id, int value, ValueSet* dest)
{
    Symbol* s = symbols->getSymbol(id);
    if (s == nullptr) {
        Trace(1, "ModelTransformer: Bad symbol id");
    }
    else if (s->parameterProperties == nullptr) {
        Trace(1, "ModelTransformer: Symbol not a parameter %s", s->getName());
    }
    else {
        const char* enumName = s->parameterProperties->getEnumName(value);
        if (enumName == nullptr) {
            Trace(1, "ModelTransformer: Unresolved enumeration %s %d", s->getName(), value);
        }
        else {
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

/**
 * Transform the ValueSet representation of an enum back to a range
 * checked ordinal that can be safely cast.
 */
int ModelTransformer::getEnum(SymbolId id, ValueSet* src)
{
    int ordinal = 0;
    
    Symbol* s = symbols->getSymbol(id);
    if (s != nullptr) {
        ParameterProperties* props = s->parameterProperties.get();

        if (props == nullptr) {
            Trace(1, "ModelTransformer: Symbol not a parameter %s", s->getName());
        }
        else if (props->type != TypeEnum) {
            Trace(1, "ModelTransformer: Symbol not a enumeration %s", s->getName());
        }
        else {
            MslValue* mv = src->get(s->name);
            if (mv == nullptr) {
                // leave it zero
            }
            else if (mv->type != MslValue::Enum && mv->type != MslValue::Int) {
                Trace(1, "ModelTransformer: Value for symbol %s not an enum or int",
                      s->getName());
            }
            else if (props->values.size() == 0) {
                Trace(1, "ModelTransformer: Unable to validate enumeration for symbol %s",
                      s->getName());
                // risky to assume it's within range, should leave zero?
                ordinal = mv->getInt();
            }
            else {
                int ival = mv->getInt();
                if (ival >= props->values.size()) {
                    Trace(1, "ModelTransformer: Parameter %s value %d out of range",
                          s->getName(), ival);
                    // could look it up by name I suppose...
                }
                else {
                    // if we have an enum symbol, do some sanity checks on it and warn
                    // but use the numeric value
                    if (mv->type == MslValue::Enum) {
                        const char* eval = mv->getString();
                        int index = props->values.indexOf(juce::String(eval));
                        if (index < 0)
                          Trace(1, "ModelTransformer: Parameter %s enumeration %s not found",
                                s->getName(), eval);
                        else if (index != ival)
                          Trace(1, "ModelTransformer: Parameter %s enumeration %s index mismatch",
                                s->getName(), eval);
                    }
                    ordinal = ival;
                }
            }
        }
    }
    return ordinal;
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

    // this copies over, but SessionClerk should almost immediately ugprade
    // to merge the contents of the referenced parameter set into the session
    // no longer have a parameter symbol for this
    const char* dp = src->getDefaultPresetName();
    if (dp != nullptr)
      values->setString("defaultPreset", dp);
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
    dest->setDefaultPresetName(values->getString("defaultPreset"));

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
    // from the Setup, this is copied over using the old name, but SessionClerk
    // will almost immediately upgrade this to trackOverlay
    const char* tp = src->getTrackPresetName();
    if (tp != nullptr)
      values->setString("trackPreset", tp);

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
        case SYNC_DEFAULT: break;
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
        case TRACK_UNIT_DEFAULT: break;
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
    dest->setTrackPresetName(values->getString("trackPreset"));

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
// Presets and ParameterSets
//
//////////////////////////////////////////////////////////////////////

/**
 * These are the rare cases where parameter name constants are necessary.
 * Use GetParameterName
 */
void ModelTransformer::transform(Preset* preset, ValueSet* set)
{
    transform(ParamSubcycles, preset->getSubcycles(), set);
    transformEnum(ParamMultiplyMode, preset->getMultiplyMode(), set);
    transformEnum(ParamShuffleMode, preset->getShuffleMode(), set);
    transformBool(ParamAltFeedbackEnable, preset->isAltFeedbackEnable(), set);
    transformEnum(ParamEmptyLoopAction, preset->getEmptyLoopAction(), set);
    transformEnum(ParamEmptyTrackAction, preset->getEmptyTrackAction(), set);
    transformEnum(ParamTrackLeaveAction, preset->getTrackLeaveAction(), set);
    transform(ParamLoopCount, preset->getLoops(), set);
    transformEnum(ParamMuteMode, preset->getMuteMode(), set);
    transformEnum(ParamMuteCancel, preset->getMuteCancel(), set);
    transformBool(ParamOverdubQuantized, preset->isOverdubQuantized(), set);
    transformEnum(ParamQuantize, preset->getQuantize(), set);
    transformEnum(ParamBounceQuantize, preset->getBounceQuantize(), set);

    transformBool(ParamRecordResetsFeedback, preset->isRecordResetsFeedback(), set);
    transformBool(ParamSpeedRecord, preset->isSpeedRecord(), set);
    transformBool(ParamRoundingOverdub, preset->isRoundingOverdub(), set);
    transformEnum(ParamSwitchLocation, preset->getSwitchLocation(), set);
    transformEnum(ParamReturnLocation, preset->getReturnLocation(), set);
    transformEnum(ParamSwitchDuration, preset->getSwitchDuration(), set);
    transformEnum(ParamSwitchQuantize, preset->getSwitchQuantize(), set);
    transformEnum(ParamTimeCopyMode, preset->getTimeCopyMode(), set);
    transformEnum(ParamSoundCopyMode, preset->getSoundCopyMode(), set);
    transformBool(ParamSwitchVelocity, preset->isSwitchVelocity(), set);
    
    transform(ParamMaxUndo, preset->getMaxUndo(), set);
    transform(ParamMaxRedo, preset->getMaxRedo(), set);
    transformBool(ParamNoFeedbackUndo, preset->isNoFeedbackUndo(), set);
    transformBool(ParamNoLayerFlattening, preset->isNoLayerFlattening(), set);
    transformBool(ParamSpeedShiftRestart, preset->isSpeedShiftRestart(), set);
    transformBool(ParamPitchShiftRestart, preset->isPitchShiftRestart(), set);
    transform(ParamSpeedStepRange, preset->getSpeedStepRange(), set);
    transform(ParamSpeedBendRange, preset->getSpeedBendRange(), set);
    transform(ParamPitchStepRange, preset->getPitchStepRange(), set);
    transform(ParamPitchBendRange, preset->getPitchBendRange(), set);
    transform(ParamTimeStretchRange, preset->getTimeStretchRange(), set);
    
    transformEnum(ParamSlipMode, preset->getSlipMode(), set);
    transform(ParamSlipTime, preset->getSlipTime(), set);
    transformEnum(ParamRecordTransfer, preset->getRecordTransfer(), set);
    transformEnum(ParamOverdubTransfer, preset->getOverdubTransfer(), set);
    transformEnum(ParamReverseTransfer, preset->getReverseTransfer(), set);
    transformEnum(ParamSpeedTransfer, preset->getSpeedTransfer(), set);
    transformEnum(ParamPitchTransfer, preset->getPitchTransfer(), set);
    transformEnum(ParamWindowSlideUnit, preset->getWindowSlideUnit(), set);
    transformEnum(ParamWindowEdgeUnit, preset->getWindowEdgeUnit(), set);
    transform(ParamWindowSlideAmount, preset->getWindowSlideAmount(), set);
    transform(ParamWindowEdgeAmount, preset->getWindowEdgeAmount(), set);

    // SpeedSequence, PitchSequence
    // These are stored in the XML as strings but get parsed into
    // a StepSequence during Preset construction
    transform(ParamSpeedSequence, preset->getSpeedSequence()->getSource(), set);
    transform(ParamPitchSequence, preset->getSpeedSequence()->getSource(), set);
}

void ModelTransformer::transform(ValueSet* set, Preset* preset)
{
    preset->setSubcycles(getInt(ParamSubcycles, set));
    preset->setMultiplyMode((ParameterMultiplyMode)getEnum(ParamMultiplyMode, set));
    preset->setShuffleMode((ShuffleMode)getEnum(ParamShuffleMode, set));
    preset->setAltFeedbackEnable(getBool(ParamAltFeedbackEnable, set));
    preset->setEmptyLoopAction((EmptyLoopAction)getEnum(ParamEmptyLoopAction, set));
    preset->setEmptyTrackAction((EmptyLoopAction)getEnum(ParamEmptyTrackAction, set));
    preset->setTrackLeaveAction((TrackLeaveAction)getEnum(ParamTrackLeaveAction, set));
    preset->setLoops(getInt(ParamLoopCount, set));
    preset->setMuteMode((ParameterMuteMode)getEnum(ParamMuteMode, set));
    preset->setMuteCancel((MuteCancel)getEnum(ParamMuteCancel, set));
    preset->setOverdubQuantized(getBool(ParamOverdubQuantized, set));
    preset->setQuantize((QuantizeMode)getEnum(ParamQuantize, set));
    preset->setBounceQuantize((QuantizeMode)getEnum(ParamBounceQuantize, set));

    preset->setRecordResetsFeedback(getBool(ParamRecordResetsFeedback, set));
    preset->setSpeedRecord(getBool(ParamSpeedRecord, set));
    preset->setRoundingOverdub(getBool(ParamRoundingOverdub, set));
    preset->setSwitchLocation((SwitchLocation)getEnum(ParamSwitchLocation, set));
    preset->setReturnLocation((SwitchLocation)getEnum(ParamReturnLocation, set));
    preset->setSwitchDuration((SwitchDuration)getEnum(ParamSwitchDuration, set));
    preset->setSwitchQuantize((SwitchQuantize)getEnum(ParamSwitchQuantize, set));
    preset->setTimeCopyMode((CopyMode)getEnum(ParamTimeCopyMode, set));
    preset->setSoundCopyMode((CopyMode)getEnum(ParamSoundCopyMode, set));
    preset->setSwitchVelocity(getBool(ParamSwitchVelocity, set));
    
    preset->setMaxUndo(getInt(ParamMaxUndo, set));
    preset->setMaxRedo(getInt(ParamMaxRedo, set));
    preset->setNoFeedbackUndo(getBool(ParamNoFeedbackUndo, set));
    preset->setNoLayerFlattening(getBool(ParamNoLayerFlattening, set));
    preset->setSpeedShiftRestart(getBool(ParamSpeedShiftRestart, set));
    preset->setPitchShiftRestart(getBool(ParamPitchShiftRestart, set));
    preset->setSpeedStepRange(getInt(ParamSpeedStepRange, set));
    preset->setSpeedBendRange(getInt(ParamSpeedBendRange, set));
    preset->setPitchStepRange(getInt(ParamPitchStepRange, set));
    preset->setPitchBendRange(getInt(ParamPitchBendRange, set));
    preset->setTimeStretchRange(getInt(ParamTimeStretchRange, set));
    
    preset->setSlipMode((SlipMode)getEnum(ParamSlipMode, set));
    preset->setSlipTime(getInt(ParamSlipTime, set));
    preset->setRecordTransfer((TransferMode)getEnum(ParamRecordTransfer, set));
    preset->setOverdubTransfer((TransferMode)getEnum(ParamOverdubTransfer, set));
    preset->setReverseTransfer((TransferMode)getEnum(ParamReverseTransfer, set));
    preset->setSpeedTransfer((TransferMode)getEnum(ParamSpeedTransfer, set));
    preset->setPitchTransfer((TransferMode)getEnum(ParamPitchTransfer, set));
    preset->setWindowSlideUnit((WindowUnit)getEnum(ParamWindowSlideUnit, set));
    preset->setWindowEdgeUnit((WindowUnit)getEnum(ParamWindowEdgeUnit, set));
    preset->setWindowSlideAmount(getInt(ParamWindowSlideAmount, set));
    preset->setWindowEdgeAmount(getInt(ParamWindowEdgeAmount, set));

    preset->setSpeedSequence(getString(ParamSpeedSequence, set));
    preset->setPitchSequence(getString(ParamPitchSequence, set));
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
