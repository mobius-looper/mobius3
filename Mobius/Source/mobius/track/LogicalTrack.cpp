
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
    cacheParameters();
    
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
            if (sets != nullptr) {
                trackOverlay = sets->find(juce::String(ovname));
                // sigh, need to return what used to be the trackPreset "ordinal" in
                // the TrackState, parameter sets don't remember their positiion so do it here
                // zero means "none"
                if (trackOverlay != nullptr) {
                    int index = sets->indexOf(trackOverlay);
                    trackOverlayNumber = index + 1;
                }
            }
        
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
    // return track->getGroup();
    return groupNumber;
}

bool LogicalTrack::isFocused()
{
    // track->isFocused();
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
    // what used to be the active preset is now handled here
    // this used to be the ordinal number of the track's Preset
    // and it always had one, now trackOverlay is optional so
    // zero needs to mean "none" in the display
    state->preset = trackOverlayNumber;
    
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
        clearBindings();
        resetSyncState();
        resetParameters();
        resolveParameterOverlays();
        track->doAction(a);
    }
    else if (sid == ParamActivePreset ||
             sid == ParamTrackOverlay) {
        // until Presets are ripped out, intercept that and use it as an alternate way
        // to select the track overlay
        // the old TrackPresetParameterType handler which will eventually receive this
        // accepts a name in the bindingArgs or an ordinal in the value

        ValueSet* overlay = nullptr;
        int overlayNumber = 0;
        ParameterSets* sets = manager->getParameterSets();
        if (strlen(a->arguments) > 0) {
            if (sets != nullptr) {
                overlay = sets->find(juce::String(a->arguments));
                if (overlay != nullptr) {
                    // need to remember it's position for the TrackState
                    int index = sets->indexOf(overlay);
                    overlayNumber = index + 1;
                }
            }
            if (overlay == nullptr)
              Trace(1, "LogicalTrack: Invalid parameter set name %s", a->arguments);
        }
        
        if (overlay == nullptr) {
            // unlike Presets which were always present, overlays are optional
            // and zero means "none"
            if (a->value == 0) {
                trackOverlay = nullptr;
                trackOverlayNumber = 0;
            }
            else {
                if (sets != nullptr) {
                    overlay = sets->getByOrdinal(a->value);
                }
                if (overlay == nullptr)
                  Trace(1, "LogicalTrack: Invalid parameter set ordinal %d", a->value);
                else
                  overlayNumber = a->value;
            }
        }

        if (overlay != nullptr) {
            trackOverlay = overlay;
            trackOverlayNumber = overlayNumber;
        }
    }
    else if (a->symbol->behavior == BehaviorActivation) {
        // the only one we support is Parameter activation which
        if (a->symbol->name.startsWith(Symbol::ActivationPrefixParameter)) {
            juce::String pname = a->symbol->name.fromFirstOccurrenceOf(Symbol::ActivationPrefixParameter, false, false);
            ValueSet* overlay = nullptr;
            ParameterSets* sets = manager->getParameterSets();
            if (sets != nullptr)
              overlay = sets->find(pname);
            if (overlay == nullptr)
              Trace(1, "LogicalTrack: Invalid parameter set name %s", pname.toUTF8());
            else
              trackOverlay = overlay;
        }
        else {
            Trace(1, "LogicalTrack: Received unsupported activation prefix %s",
                  a->symbol->getName());
        }
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
 * A few parameters are managed directly on the LogicalTrack, the reset
 * are held in the session.  The parameters that are handled here need to
 * include everything in cacheParameters.
 */
void LogicalTrack::doParameter(UIAction* a)
{
    Symbol* s = a->symbol;
    bool handled = true;
    
    switch (s->id) {
        case ParamSyncSource:
            syncSource = (SyncSource)getEnumOrdinal(s, a->value);
            break;
            
        case ParamSyncSourceAlternate:
            syncSourceAlternate = (SyncSourceAlternate)getEnumOrdinal(s, a->value);
            break;
            
        case ParamSyncUnit:
            syncUnit = (SyncUnit)getEnumOrdinal(s, a->value);
            break;
            
        case ParamTrackSyncUnit:
            trackSyncUnit = (TrackSyncUnit)getEnumOrdinal(s, a->value);
            break;
            
        case ParamLeaderTrack:
            // todo: need range checking like we do for enums
            syncLeader = a->value;
            break;
            
        case ParamTrackGroup:
            groupNumber = getGroupFromAction(a);
            break;
            
        case ParamFocus:
            focusLock = (bool)(a->value);
            break;
            
        default: handled = false; break;
    }
    
    if (!handled) {
        // before we just go slamming numbers into the bindings, need
        // to be doing the equivalent of getEnumOrdinal on these
        bindParameter(a);
        track->doAction(a);
    }
    else {
        // none of the directly cached parameters need to be sent to the
        // tracks right now, but that could change
    }
}

/**
 * Given an uncontrained number from a UIAction value, verify that
 * it fits within the enumeration range of a parameter.
 *
 * This is a generic utiliity, maybe move it to Enumerator?
 */
int LogicalTrack::getEnumOrdinal(Symbol* s, int value)
{
    int ordinal = 0;
    
    ParameterProperties* props = s->parameterProperties.get();
    if (props == nullptr) {
        Trace(1, "LogicalTrack::getEnumOrdinal Not a parameter symbol",
              s->getName());
    }
    else if (props->type == TypeEnum) {
        if (value >= 0 && value < props->values.size())
          ordinal = value;
        else
          Trace(1, "LogicalTrack::getEnumOrdinal Value out of range %s %d",
                s->getName(), value);
    }
    else {
        // this could also do range checking on numeric parameters
        Trace(1, "LogicalTrack::getEnumOrdinal not an enumerated parameter symbol %s",
              s->getName());
    }

    return ordinal;
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
 *
 * When the track is reset, the values are recached from the session.
 */
void LogicalTrack::cacheParameters()
{
    syncSource = getSyncSourceFromSession();
    syncSourceAlternate = getSyncSourceAlternateFromSession();
    syncUnit = getSyncUnitFromSession();
    trackSyncUnit = getTrackSyncUnitFromSession();
    syncLeader = sessionTrack->getInt("leaderTrack");

    // other convenient things
    groupNumber = getGroupFromSession();

    // keep the symbol table around, we're going to be needing it...
    Symbol* s = manager->getSymbols()->getSymbol(ParamFocus);
    focusLock = sessionTrack->getBool(s->name);
}

int LogicalTrack::getGroupFromSession()
{
    int gnumber = 0;

    const char* groupName = sessionTrack->getString("trackGroup");
    // since we store the name in the session, have to map it back to an ordinal
    // which requires the MobiusConfig
    if (groupName != nullptr) {
        MobiusConfig* config = manager->getConfigurationForGroups();
        int ordinal = config->getGroupOrdinal(groupName);
        if (ordinal < 0)
          Trace(1, "LogicalTrack: Invalid group name found in session %s", groupName);
        else
          gnumber = ordinal + 1;
    }

    return gnumber;
}

int LogicalTrack::getGroupFromAction(UIAction* a)
{
    int gnumber = 0;

    // todo: assumign we're dealing with numbers, but should take names
    // in the binding args
    MobiusConfig* config = manager->getConfigurationForGroups();
    if (a->value >= 0 && a->value < config->dangerousGroups.size()) {
        gnumber = a->value;
    }
    else {
        Trace(1, "LogicalTrack: Group number out of range %d", a->value);
    }
    
    return gnumber;
}

/**
 * Initialize parameters on GlobalReset or TrackReset
 *
 * This is where Track updates the input/output levels, IO ports,
 * the loop counts, and a few other things.  It caches those so it will
 * happen when the Reset function is send through the core.
 */
void LogicalTrack::resetParameters()
{
    // these were formerly handled by Track, and were sensitive
    // to "reset retain" options on the old Parameter model
    // now that they live up here, will need awareness of resetRetains as well
    
    // other convenient things
    groupNumber = getGroupFromSession();

    // keep the symbol table around, we're going to be needing it...
    Symbol* s = manager->getSymbols()->getSymbol(ParamFocus);
    focusLock = sessionTrack->getBool(s->name);

    // !! Track would formerly revert the Preset to what was in the Setup
    // on GlobalReset, or TrackReset if the special "reset retain" option was
    // off for the TrackPreset parameter
    // in the new world, this means how the trackOverlay is handled
    // we either revert to the Overlay specified in the Session or keep the current one
    // punting on this for awhile
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

int LogicalTrack::getSubcycles()
{
    int subcycles = getParameterOrdinal(ParamSubcycles);
    if (subcycles == 0) subcycles = 4;
    return subcycles;
}

//////////////////////////////////////////////////////////////////////
//
// Group 2: Things that might be in the Preset
//
// These are temporary until the session editor is fleshed out.
//
//////////////////////////////////////////////////////////////////////

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
