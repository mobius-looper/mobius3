
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/StructureDumper.h"
#include "../../model/ParameterConstants.h"
#include "../../model/SyncConstants.h"
#include "../../model/Session.h"
#include "../../model/ParameterSets.h"
#include "../../model/Symbol.h"
#include "../../model/Enumerator.h"
#include "../../model/MobiusConfig.h"
#include "../../model/ParameterProperties.h"
#include "../../model/UIParameterHandler.h"
#include "../../model/ExValue.h"

#include "../../script/MslEnvironment.h"
#include "../../script/MslBinding.h"
#include "../../script/MslValue.h"

#include "../core/Mobius.h"
#include "../midi/MidiEngine.h"
#include "../midi/MidiTrack.h"

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
}

LogicalTrack::~LogicalTrack()
{
    // track unique_ptr deletes itself
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
 * After track reorganization has finished and all tracks
 * are in place, this is called to let it process the definition
 * stored by setSession.
 */
void LogicalTrack::loadSession()
{
    if (sessionTrack == nullptr) {
        Trace(1, "LogicalTrack::loadSession Session object was not set");
        return;
    }

    resolveParameterOverlays();
    cacheSyncParameters();
    
    if (track == nullptr) {
        // this was a new logical track
        // make a new inner track using the appopriate track factory
        if (trackType == Session::TypeMidi) {
            // the engine has no state at the moment, though we may want this
            // to be where the type specific pools live
            MidiEngine engine;
            // this one will call back for the BaseScheduler and wire it in
            // with a LooperScheduler
            // not sure I like the handoff here
            track.reset(engine.newTrack(manager, this, sessionTrack));
        }
        else if (trackType == Session::TypeAudio) {
            // These should have been allocated earlier during Mobius configuration
            Trace(1, "LogicalTrack: Should have created a Mobius track by now");
        }
        else {
            Trace(1, "LogicalTrack: Unknown track type");
        }
    }
    else {
        // we've had this one before, tell it to reconfigure
        // since the inner track can always get back to the LogicalTrack we don't
        // need to pass the Session down
        track->loadSession(sessionTrack);
    }
}

/**
 * Resolve the two potential parameter overlays, one from the Session shared by all tracks
 * and another for this specific track.
 * When we get to the point where there can be more than just these two, then the ValueSets
 * should be merged into a local copy.
 */
void LogicalTrack::resolveParameterOverlays()
{
    trackOverlay = nullptr;
    ParameterSets* sets = manager->getParameterSets();
    if (sessionTrack != nullptr) {
        const char* ovname = sessionTrack->getString("trackOverlay");
        if (ovname != nullptr) {
            if (sets != nullptr)
              trackOverlay = sets->find(juce::String(ovname));
        
            if (trackOverlay == nullptr)
              Trace(1, "LogicalTrack: Invalid parameter overlay in session %s", ovname);
        }
    }
    
    sessionOverlay = nullptr;
    Session* session = manager->getSession();
    if (session != nullptr) {
        const char* ovname = session->getString("sessionOverlay");
        if (ovname != nullptr) {
            if (sets != nullptr)
              sessionOverlay = sets->find(juce::String(ovname));

            if (sessionOverlay == nullptr)
              Trace(1, "LogicalTrack: Invalid parameter overlay in session %s", ovname);
        }

        // pretty sure we used to have "defaultPreset" in the Setup which should
        // have been moved to the Session, getting tired of this shit, punt
    }
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
 * Synchronization parameters are extremely important for deciding things
 * so cache them rather than going back to the Session and the bindings every time.
 *
 * These have been duplicated at several levels, but now that LT is managing them
 * they can get them from here.  These are AUTHORITATIVE over everything above
 * and below the LT.
 *
 * This is also where temporary overrides to the values go if you ahve an action
 * or script that changes them.
 */
void LogicalTrack::cacheSyncParameters()
{
    syncSource = getSyncSourceFromSession();
    syncSourceAlternate = getSyncSourceAlternateFromSession();
    syncUnit = getSyncUnitFromSession();
    trackSyncUnit = getTrackSyncUnitFromSession();
    syncLeader = sessionTrack->getInt("leaderTrack");
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
    track->getTrackProperties(props);
}

int LogicalTrack::getGroup()
{
    return track->getGroup();
}

bool LogicalTrack::isFocused()
{
    return track->isFocused();
}

/**
 * Audio tracks are handled in bulk through Mobius
 */
void LogicalTrack::processAudioStream(MobiusAudioStream* stream)
{
    track->processAudioStream(stream);
}

bool LogicalTrack::doQuery(Query* q)
{
    return track->doQuery(q);
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
    // only MIDI tracks support notifications
    if (trackType == Session::TypeMidi) {
        track->trackNotification(notification, props);
    }
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

void LogicalTrack::refreshState(TrackState* state)
{
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

//////////////////////////////////////////////////////////////////////
//
// Actions
//
//////////////////////////////////////////////////////////////////////

/**
 * There are a few things we intercept, the reset are passed to the inner track.
 *
 * !! This is where we need to add the concept of temporary parameter bindings.
 * Core Mobius does this by maintaining a copy of the Preset on every track, need
 * to unwind that eventually.
 *
 * MidiTrack calls back to our bindParameter if it isn't one of the control parameters.
 */
void LogicalTrack::doAction(UIAction* a)
{
    SymbolId sid = a->symbol->id;

    if (sid == FuncTrackReset || sid == FuncGlobalReset) {
        clearBindings();
        resetSyncState();
        resolveParameterOverlays();
        track->doAction(a);
    }
    else if (sid == ParamActivePreset) {
        // until Presets are ripped out, intercept that and use it as an alternate way
        // to select the track overlay
        // the old TrackPresetParameterType handler which will eventually receive this
        // accepts a name in the bindingArgs or an ordinal in the value

        ValueSet* overlay = nullptr;
        ParameterSets* sets = manager->getParameterSets();
        if (strlen(a->arguments) > 0) {
            if (sets != nullptr)
              overlay = sets->find(juce::String(a->arguments));
            if (overlay == nullptr)
              Trace(1, "LogicalTrack: Invalid parameter set name %s", a->arguments);
        }

        if (overlay == nullptr) {
            if (sets != nullptr)
              overlay = sets->getByOrdinal(a->value);
            if (overlay == nullptr)
              Trace(1, "LogicalTrack: Invalid parameter set ordinal %d", a->value);
        }

        if (overlay != nullptr)
          trackOverlay = overlay;

        // also pass it along so core tracks can change their Preset
        // it will end up in the TrackPresetParameterType handler
        track->doAction(a);
    }
    else if (a->symbol->behavior == BehaviorActivation) {
        // the only one we support is Preset activation which
        // should eventually become Overlay selection
        if (a->symbol->name.startsWith(Symbol::ActivationPrefixPreset)) {

            juce::String pname = a->symbol->name.fromFirstOccurrenceOf(Symbol::ActivationPrefixPreset, false, false);
            ValueSet* overlay = nullptr;
            ParameterSets* sets = manager->getParameterSets();
            if (sets != nullptr)
              overlay = sets->find(pname);
            if (overlay == nullptr)
              Trace(1, "LogicalTrack: Invalid parameter set name %s", pname.toUTF8());
            else
              trackOverlay = overlay;

            // let Actionator do the same thing with Presets for awhile
            track->doAction(a);
        }
        else {
            Trace(1, "LogicalTrack: Received unsupported activation prefix %s",
                  a->symbol->getName());
        }
    }
    else if (a->symbol->parameterProperties == nullptr) {
        // usually a function
        track->doAction(a);
    }
    else {
        // parameters are a little harder
        // sending an action to change a parameter value establishes
        // a binding that will persist until the track is reset
        // the track mayu also choose to cache this value which makes
        // the binding redundant but be consistent about it
        // this will be noisy for track controls like input/output levels
        // consider having BaseTrack::doAction return a boolean if it decided
        // to cache the value so we don't have to
        bindParameter(a);
        track->doAction(a);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Parameters
//
//////////////////////////////////////////////////////////////////////

/**
 * Here when a track receives an action to change the value of
 * a parameter.  Tracks may choose to cache some parameters in local
 * class members, the rest will be maintained in LogicalTrack.
 *
 * Parameter bindings are temporary and normally cleared on the next Reset.
 *
 * !! holy shit, MslEnvironment object pools are NOT thread safe
 * how did you miss that?  
 */
void LogicalTrack::bindParameter(UIAction* a)
{
    SymbolId symid = a->symbol->id;
    MslBinding* existing = nullptr;
    for (MslBinding* b = bindings ; b != nullptr ; b = b->next) {
        // by convention we use the symbol id to identify bindings rather than the name
        if (b->symbolId == symid) {
            existing = b;
            break;
        }
    }

    MslValue* value = nullptr;
    if (existing != nullptr) {
        // replace the previous value
        value = existing->value;
        if (value == nullptr) {
            Trace(1, "LogicalTrack: Unexpected null value in binding");
            value = manager->getMsl()->allocValue();
            existing->value = value;
        }
    }
    else {
        value = manager->getMsl()->allocValue();
        MslBinding* b = manager->getMsl()->allocBinding();
        b->symbolId = symid;
        b->value = value;
        b->next = bindings;
        bindings = b;
    }

    // todo: only expecting ordinals right now
    value->setInt(a->value);

    // activePreset is special, store it here so we don't have to keep
    // digging it out of the binding list
    if (symid == ParamActivePreset)
      activePreset = a->value;
}

/**
 * Clear the temporary parameter bindings.
 * Sigh, this is called by the MidiTrack constructor which goes through its
 * Reset processing which wants to clear bindings.
 */
void LogicalTrack::clearBindings()
{
    while (bindings != nullptr) {
        MslBinding* next = bindings->next;
        bindings->next = nullptr;
        manager->getMsl()->free(bindings);
        bindings = next;
    }
}

///////////////////////////////////////////////////////////////////////
//
// Parameter Enumeration Conversion
//
// Now that everything is in a ValueSet, it requires conversion into
// the enumerated constant that is more convenient to use in code.
// These do that transformation and also validate that the strings and ordinals
// stored in the Session actually match the enumeration.
//
// These are only used by MIDI tracks.  Audio tracks still get them from
// the Setup which is created dynamically from the Session.
//
// !! this sucks so hard right now...
//
// The relationship between Session/TrackManager/LogicalTrack/SyncMaster
// is a fucking mess due to the old architecutre.  Now that  LogicalTrack
// is the center of the universe, we don't need as much of this.
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

int LogicalTrack::getLoopCountFromSession()
{
    int result = 2;
    Symbol* s = manager->getSymbols()->getSymbol(ParamLoopCount);
    if (s != nullptr) {
        MslValue* v = sessionTrack->get(s->name);
        if (v != nullptr) {
            result = v->getInt();
            if (result < 1) {
                // this isn't unusual if the track was created in the editor
                // and saved without filling in the form
                //Trace(1, "LogicalTrack: Malformed LoopCount parameter in session %d", number);
                result = 2;
            }
        }
    }
    return result;
}

//////////////////////////////////////////////////////////////////////
//
// Group 2: Things that might be in the Preset
//
// These are temporary until the session editor is fleshed out.
//
//////////////////////////////////////////////////////////////////////

Preset* LogicalTrack::getPreset()
{
    Preset* preset = nullptr;
    MobiusConfig* config = manager->getConfigurationForPresets();
    
    if (activePreset >= 0)
      preset = config->getPreset(activePreset);
    
    // fall back to the default
    // !! should be in the Session
    // actually Presets should go away entirely for MIDI tracks
    if (preset == nullptr)
      preset = config->getPresets();

    return preset;
}

/**
 * The primary mechanism to access parameter values from within the kernel.
 *
 * For audio tracks values come from a Preset for MIDI tracks the Session.
 *
 */
int LogicalTrack::getParameterOrdinal(SymbolId symbolId)
{
    Symbol* s = manager->getSymbols()->getSymbol(symbolId);
    int ordinal = 0;

    if (s == nullptr) {
        Trace(1, "LogicalTrack: Unmapped symbol id %d", symbolId);
    }
    else if (s->parameterProperties == nullptr) {
        Trace(1, "LogicalTrack: Symbol %s is not a parameter", s->getName());
    }
    else if (symbolId == ParamActivePreset) {
        // this one is special
        ordinal = activePreset;
    }
    else {
        MslBinding* binding = nullptr;

        // first look for a binding
        for (MslBinding* b = bindings ; b != nullptr ; b = b->next) {
            // MSL doesn't use symbol ids, only names, but since we're overloading
            // MslBinding for use in LogicalTrack, we will use ids by convention
            // hmm, cleaner if LogicalTrack just had it's own object pool for this
            if (b->symbolId == symbolId) {
                binding = b;
                break;
            }
        }

        if (binding != nullptr) {
            // this wins
            if (binding->value == nullptr) {
                // unclear whether a binding with a null value means
                // "unbound" or if it means zero?
            }
            else {
                ordinal = binding->value->getInt();
            }
        }
        else {
            // locate it in the session or overlays
            // If either a track-level or session-level overlay is present,
            // they have precedence.  This makes them resemble the old Presets.
            // If neither overlay has a value, then look in the Session::Track and
            // finally the Session
            
            MslValue* value = nullptr;
            if (trackOverlay != nullptr)
              value = trackOverlay->get(s->name);
                
            if (value == nullptr && sessionOverlay != nullptr)
              value = sessionOverlay->get(s->name);

            if (value == nullptr && sessionTrack != nullptr)
              value = sessionTrack->get(s->name);
            
            if (value == nullptr) {
                // Since we need this all the fucking time, may as well keep a handle down here
                Session* session = manager->getSession();
                if (session != nullptr)
                  value = session->get(s->name);
            }
            
            if (value != nullptr)
              ordinal = value->getInt();
            
            // temporary verification that the ParameterSets have the same
            // values as what used to be in the Preset

            if (s->parameterProperties->scope == ScopePreset) {
                
                Preset* p = getPreset();
                if (p != nullptr) {
                    ExValue exvalue;
                    UIParameterHandler::get(symbolId, p, &exvalue);
                    int presetOrdinal = exvalue.getInt();

                    if (presetOrdinal != ordinal)
                      Trace(1, "LogicalTrack: ParameterSet/Preset mismatch for %s",
                            s->getName());
                }
            }
        }
    }
    
    return ordinal;
}

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
