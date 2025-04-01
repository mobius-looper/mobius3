
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/StructureDumper.h"

#include "../../model/ParameterConstants.h"
#include "../../model/SyncConstants.h"
#include "../../model/Session.h"
#include "../../model/ParameterSets.h"
#include "../../model/UIAction.h"
#include "../../model/Query.h"
#include "../../model/Symbol.h"
#include "../../model/Enumerator.h"
#include "../../model/ParameterProperties.h"
#include "../../model/GroupDefinition.h"

#include "../../script/MslEnvironment.h"
#include "../../script/MslBinding.h"
#include "../../script/MslValue.h"

#include "../core/Mobius.h"
#include "../midi/MidiTrack.h"
// for isPlugin
#include "../MobiusInterface.h"


#include "BaseTrack.h"
#include "MobiusLooperTrack.h"
#include "TrackListener.h"
#include "TrackManager.h"

#include "LogicalTrack.h"

///////////////////////////////////////////////////////////////////////
//
// Basic Properties
//
///////////////////////////////////////////////////////////////////////

/**
 * This should not do anything fancy yet.
 * Wait until the call to loadSession if you need to do things
 * that may requires relationships with other tracks.
 */
LogicalTrack::LogicalTrack(TrackManager* tm)
{
    manager = tm;
    // won't have many of these, really just TrackManager right now
    listeners.ensureStorageAllocated(4);

    vault.initialize(tm->getSymbols(), tm->getParameterSets());
}

LogicalTrack::~LogicalTrack()
{
}

/**
 * Stupid flag to prevent Mobius core from trying to do anything
 * with it while we're in the process of deleting them.
 */
void LogicalTrack::markDying()
{
    number = 0;
}

bool LogicalTrack::isDying()
{
    return (number == 0);
}

/**
 * Assigning the session just happens during track organization
 * by TrackManager.  You do not ACT on it yet.  This only happens
 * when tracks are created.
 */
void LogicalTrack::setSession(Session::Track* trackdef, int n)
{
    sessionTrack = trackdef;
    number = n;
    trackType = sessionTrack->type;
}

Session::Track* LogicalTrack::getSession()
{
    return sessionTrack;
}

int LogicalTrack::getNumber()
{
    return number;
}

Session::TrackType LogicalTrack::getType()
{
    return trackType;
}

/**
 * This happens during TrackManager::configureTracks after we've fleshed
 * out the LogicalTrack array and want to start making or updating the BaseTracks.
 * Because core tracks are handled in bulk rather than one at a time like
 * MIDI tracks, we need to have the parameter caches refreshed before
 * we start touching core tracks.
 *
 * Kind of sucks, would be nice if Mobius tracks and MIDI tracks could be initialized
 * the same way.
 */
void LogicalTrack::prepareParameters()
{
    if (sessionTrack == nullptr) {
        Trace(1, "LogicalTrack::loadSession Session object was not set");
        return;
    }

    cacheParameters(false);
}

/**
 * After track reorganization has finished and all tracks
 * are in place, this is called to send the session to the tracks.
 * Core tracks will already have been initialized by TrackManager::configureMobiusTracks
 * so we really just need to deal with MIDI or other tracks that can be dealt
 * with one at a time.
 */
void LogicalTrack::loadSession()
{
    if (sessionTrack == nullptr) {
        Trace(1, "LogicalTrack::loadSession Session object was not set");
        return;
    }

    // ugh, interface here is messy
    // 
    vault.loadSession(sessionTrack->getSession(), sessionTrack);

    // now doen earlier in prepareParameters
    //cacheParameters(false);

    if (track == nullptr) {
        // this was a new logical track
        // make a new inner track using the appopriate track factory
        if (trackType == Session::TypeMidi) {
            // this one will call back for the BaseScheduler and wire it in
            // with a LooperScheduler
            // not sure I like the handoff here
            track.reset(new MidiTrack(manager, this));
        }
        else if (trackType == Session::TypeAudio) {
            // These should have been allocated earlier during Mobius configuration
            Trace(1, "LogicalTrack: Should have created a Mobius track by now");
        }
        else {
            Trace(1, "LogicalTrack: Unknown track type");
        }
    }

    // only need to call refreshParameters for non-core tracks right now,
    // it doesn't hurt but it's redundant
    if (trackType == Session::TypeMidi)
      track->refreshParameters();
}

/**
 * Return the wrapped MobiusLooperTrack for this logical track.
 * If one does not exist, create a stub.
 */
MobiusLooperTrack* LogicalTrack::getMobiusTrack()
{
    MobiusLooperTrack* mlt = nullptr;
    if (trackType == Session::TypeAudio) {
        if (track == nullptr) {
            mlt = new MobiusLooperTrack(manager, this);
            track.reset(mlt);
        }
        else {
            mlt = static_cast<MobiusLooperTrack*>(track.get());
        }
    }
    return mlt;
}


/**
 * Hack for the SelectTrack case where we need to assemble a UIAction
 * that uses the core track number as an argument.
 * No good way to get this without adding another virtual to BaseTrack.
 */
int LogicalTrack::getEngineNumber()
{
    int engineNumber = number;
    if (trackType == Session::TypeAudio) {
        MobiusLooperTrack* mlt = static_cast<MobiusLooperTrack*>(track.get());
        engineNumber = mlt->getCoreTrackNumber();
    }
    return engineNumber;
}

int LogicalTrack::getSessionId()
{
    // mobius tracks won't have a Session and therefore won't have
    // correlation ids, but it doesn't matter since we rebuild them every time
    return (sessionTrack != nullptr) ? sessionTrack->id : 0;
}

//////////////////////////////////////////////////////////////////////
//
// Generic Operations
//
//////////////////////////////////////////////////////////////////////

void LogicalTrack::getTrackProperties(TrackProperties& props)
{
    // this we manage
    props.subcycles = getSubcycles();
    
    track->getTrackProperties(props);
}

int LogicalTrack::getGroup()
{
    return groupNumber;
}

bool LogicalTrack::isFocused()
{
    return focusLock;
}

/**
 * Audio tracks are handled in bulk through Mobius
 */
void LogicalTrack::processAudioStream(MobiusAudioStream* stream)
{
    track->processAudioStream(stream);
}

/**
 * Only MIDI tracks need events right now
 */
void LogicalTrack::midiEvent(MidiEvent* e)
{
    // only MIDI tracks care about this, though I guess the others could just ignore it
    if (trackType == Session::TypeMidi)
      track->midiEvent(e);
}

void LogicalTrack::trackNotification(NotificationId notification, TrackProperties& props)
{
    track->trackNotification(notification, props);
}

/**
 * This is intended for waits that are normaly attached to another scheduled
 * event, or scheduled pending waiting for actiation.
 * Midi tracks use the local scheduler.
 * Audio tracks use...what exactly?
 */
bool LogicalTrack::scheduleWait(MslWait* w)
{
    (void)w;
    // todo: create an EventWait with this wait object
    // mark it pending, have beginAudioBlock look for it
    return false;
}

int LogicalTrack::scheduleFollowerEvent(QuantizeMode q, int followerTrack, int eventId)
{
    return track->scheduleFollowerEvent(q, followerTrack, eventId);
}

bool LogicalTrack::scheduleWait(TrackWait& wait)
{
    return track->scheduleWait(wait);
}

void LogicalTrack::cancelWait(TrackWait& wait)
{
    track->cancelWait(wait);
}

void LogicalTrack::finishWait(TrackWait& wait)
{
    track->finishWait(wait);
}

void LogicalTrack::dump(StructureDumper& d)
{
    track->dump(d);
}

/**
 * If this track is capable of responding to MSL, return it.
 */
MslTrack* LogicalTrack::getMslTrack()
{
    return track->getMslTrack();
}

/**
 * Special so TrackManager can go direct to a MidiTrack
 * to call loadLoop.
 * Needs thought but I don't want to clutter up every interface
 * with something so MIDI specific.
 * It's kind of like getMslTrack, what's a good way to have track type
 * specific extensions that we can directly access?
 */
MidiTrack* LogicalTrack::getMidiTrack()
{
    MidiTrack* mt = nullptr;
    if (trackType == Session::TypeMidi)
      mt = static_cast<MidiTrack*>(track.get());
    return mt;
}

//////////////////////////////////////////////////////////////////////
//
// Synchronized Recording State
//
//////////////////////////////////////////////////////////////////////

void LogicalTrack::resetSyncState()
{
    syncRecording = false;
    syncRecordStarted = false;
    syncRecordFreeStart = false;
    syncRecordElapsedFrames = 0;
    syncFinalized = false;
    syncStartUnit = SyncUnitNone;
    syncRecordUnit = SyncUnitNone;
    syncElapsedUnits = 0;
    syncElapsedBeats = 0;
    syncGoalUnits = 0;
    // might want an option for this to be preserved
    syncUnitLength = 0;
}

void LogicalTrack::syncEvent(class SyncEvent* e)
{
    track->syncEvent(e);
}

void LogicalTrack::setUnitLength(int l)
{
    syncUnitLength = l;
}

int LogicalTrack::getUnitLength()
{
    return syncUnitLength;
}

int LogicalTrack::getSyncLength()
{
    return track->getSyncLength();
}

int LogicalTrack::getSyncLocation()
{
    return track->getSyncLocation();
}

bool LogicalTrack::isSyncRecording()
{
    return syncRecording;
}

void LogicalTrack::setSyncRecording(bool b)
{
    syncRecording = b;
    if (!b) {
        // clear this too since it is no longer relevant
        syncRecordStarted = false;
        // what about the pulse units?
    }
}

bool LogicalTrack::isSyncRecordStarted()
{
    return syncRecordStarted;
}

void LogicalTrack::setSyncRecordStarted(bool b)
{
    syncRecordStarted = b;
}

bool LogicalTrack::isSyncRecordFreeStart()
{
    return syncRecordFreeStart;
}

void LogicalTrack::setSyncRecordFreeStart(bool b)
{
    syncRecordFreeStart = b;
}

int LogicalTrack::getSyncRecordElapsedFrames()
{
    return syncRecordElapsedFrames;
}

void LogicalTrack::setSyncRecordElapsedFrames(int f)
{
    syncRecordElapsedFrames = f;
}

bool LogicalTrack::isSyncFinalized()
{
    return syncFinalized;
}

void LogicalTrack::setSyncFinalized(bool b)
{
    syncFinalized = b;
}

SyncUnit LogicalTrack::getSyncStartUnit()
{
    return syncStartUnit;
}

void LogicalTrack::setSyncStartUnit(SyncUnit unit)
{
    syncStartUnit = unit;
}

SyncUnit LogicalTrack::getSyncRecordUnit()
{
    return syncRecordUnit;
}

void LogicalTrack::setSyncRecordUnit(SyncUnit unit)
{
    syncRecordUnit = unit;
}

void LogicalTrack::setSyncElapsedUnits(int i)
{
    syncElapsedUnits = i;
}

int LogicalTrack::getSyncElapsedUnits()
{
    return syncElapsedUnits;
}

void LogicalTrack::setSyncElapsedBeats(int i)
{
    syncElapsedBeats = i;
}

int LogicalTrack::getSyncElapsedBeats()
{
    return syncElapsedBeats;
}

void LogicalTrack::setSyncGoalUnits(int i)
{
    syncGoalUnits = i;
}

int LogicalTrack::getSyncGoalUnits()
{
    return syncGoalUnits;
}

Symbol* LogicalTrack::getSymbol(SymbolId id)
{
    // we're going to need this all the time, just bring
    // SymbolTable inside
    return manager->getSymbols()->getSymbol(id);
}

//////////////////////////////////////////////////////////////////////
//
// Parameter Cache
//
//////////////////////////////////////////////////////////////////////

/**
 * Synchronization parameters are extremely important for deciding things
 * so cache them rather than going back to the Session and the bindings every time.
 *
 * These have been duplicated at several levels, but now that LT is managing them
 * they can get them from here.  These are AUTHORITATIVE over everything above
 * and below the LT.
 *
 * This is also where temporary overrides to the values go if you ahve an action
 * or script that changes them.
 *
 * The reset flag is false if this is the result of a session load, and true
 * if this is the result of a TrackReset or GlobalReset.  Most parameters return
 * to their session values on reset, except for a few that had the "reset retain" option.
 */
void LogicalTrack::cacheParameters(bool reset)
{
    // todo: needs lots of work
    if (reset)
      vault.resetLocal();
    
    syncSource = getSyncSourceFromSession();
    syncSourceAlternate = getSyncSourceAlternateFromSession();
    syncUnit = getSyncUnitFromSession();
    trackSyncUnit = getTrackSyncUnitFromSession();
    syncLeader = sessionTrack->getInt(ParamLeaderTrack);

    // other convenient things
    groupNumber = getGroupFromSession();
    focusLock = sessionTrack->getBool(ParamFocus);

    // handle the virtual port swap
    if (manager->getContainer()->isPlugin()) {
        inputPort = getParameterOrdinal(ParamPluginInputPort);
        outputPort = getParameterOrdinal(ParamPluginOutputPort);
    }
    else {
        inputPort = getParameterOrdinal(ParamAudioInputPort);
        outputPort = getParameterOrdinal(ParamAudioOutputPort);
    }
}

int LogicalTrack::getGroupFromSession()
{
    int gnumber = 0;

    const char* groupName = sessionTrack->getString(ParamTrackGroup);
    // since we store the name in the session, have to map it back to an ordinal
    // which requires the GroupDefinitions
    if (groupName != nullptr) {
        GroupDefinitions* groups = manager->getGroupDefinitions();
        int index = groups->getGroupIndex(groupName);
        if (index < 0)
          Trace(1, "LogicalTrack: Invalid group name found in session %s", groupName);
        else
          gnumber = index + 1;
    }

    return gnumber;
}

int LogicalTrack::getGroupFromAction(UIAction* a)
{
    int gnumber = 0;

    // todo: assumign we're dealing with numbers, but should take names
    // in the binding args
    // number is 1 based with 0 meaning "none"
    GroupDefinitions* groups = manager->getGroupDefinitions();
    if (a->value >= 0 && a->value <= groups->groups.size()) {
        gnumber = a->value;
    }
    else {
        Trace(1, "LogicalTrack: Group number out of range %d", a->value);
    }
    
    return gnumber;
}

/**
 * State refresh is closely related to how parameters are cached.
 * Most of it is handled by the BaseTrack, but we contribute the
 * things we manage.
 */
void LogicalTrack::refreshState(TrackState* state)
{
    // !! todo: old name, revisit
    state->preset = vault.getTrackOverlayNumber();
    state->subcycles = getSubcycles();
    state->focus = focusLock;
    state->group = groupNumber;
    
    track->refreshState(state);
}

void LogicalTrack::refreshPriorityState(PriorityState* state)
{
    track->refreshPriorityState(state);
}

void LogicalTrack::refreshFocusedState(FocusedTrackState* state)
{
    track->refreshFocusedState(state);
}

//////////////////////////////////////////////////////////////////////
//
// Actions
//
//////////////////////////////////////////////////////////////////////

/**
 * A few functions are intercepted here, most are passed along to the BaseTrack.
 *
 * Parameters are entirely handled here, BaseTracks are informed only if they wish
 * to cache values, they can't maintain a value that is different from the LT.
 *
 * Activations are entirely handled here.
 */
void LogicalTrack::doAction(UIAction* a)
{
    SymbolId sid = a->symbol->id;

    if (sid == FuncTrackReset || sid == FuncGlobalReset) {
        resetSyncState();
        cacheParameters(true);
        track->doAction(a);
    }
    else if (sid == FuncFocusLock) {
        focusLock = !focusLock;
        // reflect it in the vault for query
        vault.setOrdinal(ParamFocus, focusLock);
    }
    else if (sid == FuncTrackGroup) {
        doTrackGroup(a);
        vault.setOrdinal(ParamTrackGroup, groupNumber);
    }
    else if (a->symbol->behavior == BehaviorActivation) {
        // The only Activation supported at Kernel level is
        // the track overlay
        if (a->symbol->name.startsWith(Symbol::ActivationPrefixOverlay)) {
            vault.doAction(a);
        }
        else {
            Trace(1, "LogicalTrack: Received unsupported activation prefix %s",
                  a->symbol->getName());
        }
    }
    else if (sid == ParamInputLatency || sid == ParamOutputLatency) {
        // might want these actionable someday
        Trace(1, "LogicalTrack: Action on latencies");
    }
    else if (a->symbol->parameterProperties == nullptr) {
        // must be a function
        track->doAction(a);
    }
    else {
        doParameter(a);
    }
}

/**
 * Process a parameter action.
 *
 * The new parameter value will either be cached directly in LT fields,
 * or added to the binding list.
 *
 * Only a few need to be passed through to the BaseTrack, but send all of them
 * and let the tracks sort it out.  The alternative would just be to change the
 * value here and call BaseTrack::refreshParameters.
 *
 * A few are LT level only and do not need to be sent down.
 */
void LogicalTrack::doParameter(UIAction* a)
{
    // everything passes through the vault
    vault.doAction(a);

    // some of these have local caches
    // probably don't need all of these
    Symbol* s = a->symbol;
    bool local = true;
    switch (s->id) {

        case ParamTrackGroup:
            groupNumber = vault.getOrdinal(s);
            break;

        case ParamFocus:
            focusLock = (bool)(vault.getOrdinal(s));
            break;
            
        case ParamSyncSource:
            syncSource = (SyncSource)(vault.getOrdinal(s));
            break;
            
        case ParamSyncSourceAlternate:
            syncSourceAlternate = (SyncSourceAlternate)(vault.getOrdinal(s));
            break;
            
        case ParamSyncUnit:
            syncUnit = (SyncUnit)(vault.getOrdinal(s));
            break;
            
        case ParamTrackSyncUnit:
            trackSyncUnit = (TrackSyncUnit)(vault.getOrdinal(s));
            break;
            
        case ParamLeaderTrack:
            syncLeader = vault.getOrdinal(s);
            break;
            
        case ParamInputPort:
            inputPort = vault.getOrdinal(s);
            break;
            
        case ParamOutputPort:
            outputPort = vault.getOrdinal(s);
            break;

        default: {
            local = false;
        }
            break;
    }

    // the !local optimization is minor, BaseTracks will ignore most
    // things anyway
    if (!local)
      track->doAction(a);
}

/**
 * This is a weird one.
 * EDP had an option that when entering Record mode it would put the feedback
 * back up to 127.  This was implemented with an obscure option
 * RecordResetsFeedback.
 *
 * Now that we manage what the feedback value is, the core Record function implementation
 * needs to call back up here to ask us to reset it, it can't just slam a value into
 * the track without our knowledge.
 */
int LogicalTrack::unbindFeedback()
{
    int current = getParameterOrdinal(ParamFeedback);

    // ugh: punting on this since the binding list is too horrible to deal with
    // revisit when this gets converted to a binding array like you thought about
    Trace(1, "LogicalTrack::unbindFeedback not implemented");

    return current;
}

//////////////////////////////////////////////////////////////////////
//
// Track Groups
//
//////////////////////////////////////////////////////////////////////

/**
 * Handler for the FuncTrackGroup action.
 *
 * This replicates what used to be down in core/functions/TrackGroup
 */
void LogicalTrack::doTrackGroup(UIAction* a)
{
    // this was sustainable='true' for a time, I vaguely remember longPress
    // being used to remove the group assignment, not supported at the moment
    // but if you bring that back, need to ignore up transitions here
    if (!a->sustainEnd) {
        // to allow longPress
        GroupDefinitions* groups = manager->getGroupDefinitions();

        // binding args can be used for special commands as well as names
        if (strlen(a->arguments) > 0) {
            groupNumber = parseGroupActionArgument(groups, a->arguments);
        }
        else if (a->value >= 0 && a->value <= groups->groups.size()) {
            groupNumber = a->value;
        }
        else {
            Trace(1, "LogicalTrack: Group number out of range %d", a->value);
        }
    }
}

/**
 * Here we have a string group specifier from the binding argument.
 */
int LogicalTrack::parseGroupActionArgument(GroupDefinitions* groups, const char* s)
{
    int group = 0;

    if (strlen(s) > 0) {
        
        int gnumber = 1;
        for (auto g : groups->groups) {
            // what about case on these?
            if (g->name.equalsIgnoreCase(s)) {
                group = gnumber;
                break;
            }
            gnumber++;
        }

        if (group == 0) {
            // see if it looks like a number in range
            if (IsInteger(s)) {
                group = ToInt(s);
                if (group < 1 || group > groups->groups.size()) {
                    Trace(1, "LogicalTrack: Group number out of range %d", group);
                    group = 0;
                }
            }
        }
        
        if (group == 0) {
            // name didn't match
            // accept a few cycle control keywords
            int delta = 0;
            if ((strcmp(s, "cycle") == 0) || (strcmp(s, "next") == 0)) {
                delta = 1;
            }
            else if (strcmp(s, "prev") == 0) {
                delta = -1;
            }
            else if (strcmp(s, "clear") == 0) {
                // leave at zero to clear
            }
            else {
                Trace(1, "LogicalTrack: Invalid group name %s", s);
            }

            if (delta != 0) {
                group = groupNumber + delta;
                if (group > groups->groups.size())
                  group = 0;
                else if (group < 0)
                  group = groups->groups.size();
            }
        }
    }
    
    return group;
}

//////////////////////////////////////////////////////////////////////
//
// Query
//
//////////////////////////////////////////////////////////////////////

/**
 * The LogicalTrack is authorative over parameter values.
 * BaseTracks may cache them but they cannot have values that differ.
 */
bool LogicalTrack::doQuery(Query* q)
{
    //return track->doQuery(q);
    q->value = getParameterOrdinal(q->symbol->id);

    // assuming we're at the end of the query probe chain and don't
    // have to bother with returning if this was actually a parameter or not
    return true;
}

/**
 * This is the most important function for parameter access by BaseTracks.
 *
 * Whenever a track needs the value of a parameter it MUST call up  to this
 * which undestands how they are organized in the session and deals with
 * overlays and action bindings.
 *
 * Tracks may choose to cache some of their parameters, but if they do they
 * MUST respond to a refreshParameters notification and call back to this to
 * refresh their cache.
 *
 * Anything that is maintained in an LT parameter cache needs to be handled
 * here rather than reversing to the Session.
 */
int LogicalTrack::getParameterOrdinal(SymbolId symbolId)
{
    Symbol* s = getSymbol(symbolId);
    int ordinal = 0;

    if (s == nullptr) {
        Trace(1, "LogicalTrack: Unmapped symbol id %d", symbolId);
    }
    else if (s->parameterProperties == nullptr) {
        Trace(1, "LogicalTrack: Symbol %s is not a parameter", s->getName());
    }
    else {

        bool special = true;
        switch (symbolId) {
            
            case ParamInputPort:
                ordinal = inputPort;
                break;
                
            case ParamOutputPort:
                ordinal = outputPort;
                break;

            case ParamInputLatency:
                ordinal = manager->getInputLatency();
                break;

            case ParamOutputLatency:
                ordinal = manager->getOutputLatency();
                break;
                
            default: special = false; break;
        }

        if (!special) {
            ordinal = vault.getOrdinal(s);
        }
    }
    
    return ordinal;
}

///////////////////////////////////////////////////////////////////////
//
// Parameter Enumeration Conversion
//
// These are older convenience functions used by MidiTrack that
// are not as necessary any more.  Weed these...
//
// Now that everything is in a ValueSet, it requires conversion into
// the enumerated constant that is more convenient to use in code.
// These do that transformation and also validate that the strings and ordinals
// stored in the Session actually match the enumeration.
//
// These are only used by MIDI tracks.  Audio tracks still get them from
// the Setup which is created dynamically from the Session.
//
//////////////////////////////////////////////////////////////////////

SyncSource LogicalTrack::getSyncSourceFromSession()
{
    return (SyncSource)Enumerator::getOrdinal(manager->getSymbols(),
                                              ParamSyncSource,
                                              sessionTrack->getParameters(),
                                              SyncSourceNone);
}

SyncSourceAlternate LogicalTrack::getSyncSourceAlternateFromSession()
{
    return (SyncSourceAlternate)Enumerator::getOrdinal(manager->getSymbols(),
                                                       ParamSyncSourceAlternate,
                                                       sessionTrack->getParameters(),
                                                       SyncAlternateTrack);
}

SyncUnit LogicalTrack::getSyncUnitFromSession()
{
    return (SyncUnit)Enumerator::getOrdinal(manager->getSymbols(),
                                            ParamSyncUnit,
                                            sessionTrack->getParameters(),
                                            SyncUnitBar);
}

TrackSyncUnit LogicalTrack::getTrackSyncUnitFromSession()
{
    return (TrackSyncUnit)Enumerator::getOrdinal(manager->getSymbols(),
                                                 ParamTrackSyncUnit,
                                                 sessionTrack->getParameters(),
                                                 TrackUnitLoop);
}

LeaderType LogicalTrack::getLeaderTypeFromSession()
{
    return (LeaderType)Enumerator::getOrdinal(manager->getSymbols(),
                                              ParamLeaderType,
                                              sessionTrack->getParameters(),
                                              LeaderNone);
}

LeaderLocation LogicalTrack::getLeaderSwitchLocationFromSession()
{
    return (LeaderLocation)Enumerator::getOrdinal(manager->getSymbols(),
                                                  ParamLeaderSwitchLocation,
                                                  sessionTrack->getParameters(),
                                                  LeaderLocationNone);
}

/**
 * !! The loop count was formerly in the Preset which
 * is now the Session::Track with or without a trackOverlay.
 *
 * I'd really rather these be in the special case of track parameters that
 * don't use overlays.
 */
int LogicalTrack::getLoopCountFromSession()
{
    int result = 2;

    // it is not unusual for this to be zero if the track was created in the
    // editor and saved without filling in the form, default to 2
    int count = getParameterOrdinal(ParamLoopCount);
    if (count > 1) {

        // todo: core tracks have their own limit on this, need to decide
        // where this lives
        int max = 16;
        if (count > max) {
            Trace(1, "LogicalTrack: Loop count in session out of range %d", count);
            count = max;
        }
        
        result = count;
    }

    return result;
}

/**
 * This is part of the MslTrack interface, and used in a lot of places so
 * give it a special accessor.
 */
int LogicalTrack::getSubcycles()
{
    int subcycles = getParameterOrdinal(ParamSubcycles);

    // this is also commonly left zero by the session editor
    // various levels REALLY expect this to be non-zero so default it
    if (subcycles == 0) subcycles = 4;
    
    return subcycles;
}

//////////////////////////////////////////////////////////////////////
//
// Group 2: Things that might be in the Preset
//
// These are temporary until the session editor is fleshed out.
//
// todo: more weeding to do
//
//////////////////////////////////////////////////////////////////////

ParameterMuteMode LogicalTrack::getMuteMode()
{
    return (ParameterMuteMode)getParameterOrdinal(ParamMuteMode);
}

SwitchLocation LogicalTrack::getSwitchLocation()
{
    return (SwitchLocation)getParameterOrdinal(ParamSwitchLocation);
}

SwitchDuration LogicalTrack::getSwitchDuration()
{
    return (SwitchDuration)getParameterOrdinal(ParamSwitchDuration);
}

SwitchQuantize LogicalTrack::getSwitchQuantize()
{
    return (SwitchQuantize)getParameterOrdinal(ParamSwitchQuantize);
}

QuantizeMode LogicalTrack::getQuantizeMode()
{
    return (QuantizeMode)getParameterOrdinal(ParamQuantize);
}

EmptyLoopAction LogicalTrack::getEmptyLoopAction()
{
    return (EmptyLoopAction)getParameterOrdinal(ParamEmptyLoopAction);
}


//////////////////////////////////////////////////////////////////////
//
// Notifier State
//
//////////////////////////////////////////////////////////////////////

void LogicalTrack::addTrackListener(TrackListener* l)
{
    listeners.add(l);
}

void LogicalTrack::removeTrackListener(TrackListener* l)
{
    listeners.removeAllInstancesOf(l);
}

/**
 * Notify any listeners of something that happened in another track.
 *
 * todo: TrackListener is probably too much of an abstraction.  If the
 * only thing that can listen on another track is another track, then
 * we can skip the TrackListener interface and just call the other
 * track directly.
 *
 * see comments in TrackManager::loadSession for why this is over-engineered
 */
void LogicalTrack::notifyListeners(NotificationId id, TrackProperties& props)
{
    for (auto l : listeners) {
        l->trackNotification(id, props);
    }
}

//////////////////////////////////////////////////////////////////////
//
// TimeSlicer State
//
//////////////////////////////////////////////////////////////////////

/**
 * State flags used by TimeSlicer to order track advance.
 * Saves some annoying array sorting and cycle detection.
 */
bool LogicalTrack::isVisited()
{
    return visited;
}

void LogicalTrack::setVisited(bool b)
{
    visited = b;
}

bool LogicalTrack::isAdvanced()
{
    return advanced;
}

void LogicalTrack::setAdvanced(bool b)
{
    advanced = b;
}

//////////////////////////////////////////////////////////////////////
//
// Sync State
//
// This evolved away from Leader/Follower in Pulsator and we're keeping this
// at several levels now.   Don't need so much duplication.
//
//////////////////////////////////////////////////////////////////////

SyncSource LogicalTrack::getSyncSourceNow()
{
    return syncSource;
}

SyncUnit LogicalTrack::getSyncUnitNow()
{
    return syncUnit;
}

TrackSyncUnit LogicalTrack::getTrackSyncUnitNow()
{
    return trackSyncUnit;
}

int LogicalTrack::getSyncLeaderNow()
{
    return syncLeader;
}

Pulse* LogicalTrack::getLeaderPulse()
{
    return &leaderPulse;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
