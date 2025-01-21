
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/StructureDumper.h"
#include "../../model/ParameterConstants.h"
#include "../../model/Session.h"
#include "../../model/Symbol.h"
#include "../../model/Enumerator.h"
#include "../../model/MobiusConfig.h"
#include "../../model/ParameterProperties.h"
#include "../../model/UIParameterHandler.h"
#include "../../model/ExValue.h"

#include "../../sync/SyncConstants.h"

#include "../../script/MslEnvironment.h"
#include "../../script/MslBinding.h"
#include "../../script/MslValue.h"

#include "../core/Mobius.h"
#include "../midi/MidiEngine.h"
#include "../midi/MidiTrack.h"

#include "BaseTrack.h"
#include "MobiusLooperTrack.h"
#include "TrackManager.h"

#include "LogicalTrack.h"

LogicalTrack::LogicalTrack(TrackManager* tm)
{
    manager = tm;
}

LogicalTrack::~LogicalTrack()
{
    // track unique_ptr deletes itself
}

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

/**
 * Normal initialization driven from the Session.
 */
void LogicalTrack::loadSession(Session::Track* trackdef)
{
    // assumes it is okay to hang onto this until the next one is loaded
    sessionTrack = trackdef;
    trackType = trackdef->type;

    if (track == nullptr) {
        // make a new one using the appopriate track factory
        if (trackType == Session::TypeMidi) {
            // the engine has no state at the moment, though we may want this
            // to be where the type specific pools live
            MidiEngine engine;
            // this one will call back for the BaseScheduler and wire it in
            // with a LooperScheduler
            // not sure I like the handoff here
            track.reset(engine.newTrack(manager, this, trackdef));
        }
        else if (trackType == Session::TypeAudio) {
            // core tracks are special
            // these have to be done in order at the front
            // if they can be out of order then will need to "allocate" engine
            // numbers as they are encountered
            engineNumber = trackdef->number - 1;
            Mobius* m = manager->getAudioEngine();
            Track* mt = m->getTrack(engineNumber);
            track.reset(new MobiusLooperTrack(manager, this, m, mt));
            // Mobius tracks don't use the Session so we have to put the number
            // there for it
            track->setNumber(trackdef->number);
        }
        else {
            Trace(1, "LogicalTrack: Unknown track type");
        }
    }
    else {
        track->loadSession(trackdef);
        // sanity check on numbers
        if (track->getNumber() != trackdef->number)
          Trace(1, "LogicalTrack::loadSession Track number mismatch");
    }
}

Session::TrackType LogicalTrack::getType()
{
    return trackType;
}

int LogicalTrack::getNumber()
{
    return (track != nullptr) ? track->getNumber() : 0;
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

void LogicalTrack::doAction(UIAction* a)
{
    track->doAction(a);
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

void LogicalTrack::syncPulse(Pulse* p)
{
    track->syncPulse(p);
}

//////////////////////////////////////////////////////////////////////
//
// Parameters
//
// This grew out of Valuator, which turned out not to be necessary for
// non-core tracks since LogicalTrack provices the place to hang transient
// parameter bindings and is more easilly accessible everywhere.
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
 * Reset processing which wants to clear bindings.  Valuator will not have been
 * initialized yet.
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
//////////////////////////////////////////////////////////////////////

SyncSource LogicalTrack::getSyncSource()
{
    return (SyncSource)Enumerator::getOrdinal(manager->getSymbols(),
                                              ParamSyncSource,
                                              sessionTrack->getParameters(),
                                              SyncSourceNone);
}

SyncUnit LogicalTrack::getSyncUnit()
{
    return (SyncUnit)Enumerator::getOrdinal(manager->getSymbols(),
                                            ParamSyncUnit,
                                            sessionTrack->getParameters(),
                                            SyncUnitBeat);
}

TrackSyncUnit LogicalTrack::getTrackSyncUnit()
{
    return (TrackSyncUnit)Enumerator::getOrdinal(manager->getSymbols(),
                                                 ParamTrackSyncUnit,
                                                 sessionTrack->getParameters(),
                                                 TrackUnitLoop);
}

LeaderType LogicalTrack::getLeaderType()
{
    return (LeaderType)Enumerator::getOrdinal(manager->getSymbols(),
                                              ParamLeaderType,
                                              sessionTrack->getParameters(),
                                              LeaderNone);
}

LeaderLocation LogicalTrack::getLeaderSwitchLocation()
{
    return (LeaderLocation)Enumerator::getOrdinal(manager->getSymbols(),
                                                  ParamLeaderSwitchLocation,
                                                  sessionTrack->getParameters(),
                                                  LeaderLocationNone);
}

int LogicalTrack::getLoopCount()
{
    int result = 2;
    Symbol* s = manager->getSymbols()->getSymbol(ParamLoopCount);
    if (s != nullptr) {
        MslValue* v = sessionTrack->get(s->name);
        if (v != nullptr) {
            result = v->getInt();
            if (result < 1) {
                Trace(1, "LogicalTrack: Malformed LoopCount parameter in session %d",
                      track->getNumber());
                result = 1;
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
    
    if (activePreset >= 0)
      preset = manager->getConfiguration()->getPreset(activePreset);
    
    // fall back to the default
    // !! should be in the Session
    // actually Presets should go away entirely for MIDI tracks
    if (preset == nullptr)
      preset = manager->getConfiguration()->getPresets();

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
            // no track bindings, look in the value containers

            // ugliness: until the Session transition is complete, fall back to the
            // Preset if there is no value in the Session
            
            bool foundInSession = false;
            if (trackType == Session::TypeMidi && sessionTrack != nullptr) {
                ValueSet* params = sessionTrack->getParameters();
                if (params != nullptr) {
                    MslValue* v = params->get(s->name);
                    if (v != nullptr) {
                        ordinal = v->getInt();
                        foundInSession = true;
                    }
                }
            }

            if (!foundInSession) {

                // wasn't in the session, fall back to the old model

                switch (s->parameterProperties->scope) {
                    case ScopeGlobal: {
                        // MidiTrack should be getting these from the Session
                        //Trace(1, "LogicalTrack: Kernel attempt to access global parameter %s",
                        //s->getName());
                    }
                        break;
                    case ScopePreset: {
                        Preset* p = getPreset();
                        if (p != nullptr) {
                            ExValue value;
                            UIParameterHandler::get(symbolId, p, &value);
                            ordinal = value.getInt();
                        }
                    }
                        break;
                    case ScopeSetup: {
                        // if 
                                // not sure why MidiTrack would want things here
                                //Trace(1, "LogicalTrack: Kernel attempt to access track parameter %s",
                                //s->getName());
                                }
                        break;
                    case ScopeTrack: {
                        // mostly for levels which MidiTrack should be intercepting
                        //Trace(1, "LogicalTrack: Kernel attempt to access track parameter %s",
                        //s->getName());
                    }
                        break;
                    case ScopeUI: {
                        // not expecting this from kernel tracks
                        //Trace(1, "LogicalTrack: Kernel attempt to access UI parameter %s",
                        //s->getName());
                    }
                        break;
                    case ScopeSync: {
                        // also supposed to be in Session
                    }
                        break;
                    case ScopeNone: {
                        Trace(1, "LogicalTrack: Kernel attempt to access unscoped parameter %s",
                              s->getName());
                    }
                        break;

                    case ScopeSession:
                    case ScopeSessionTrack:
                        // these would have been found in the Session
                        // if they were there, avoid unhandled switch warning
                        break;
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
