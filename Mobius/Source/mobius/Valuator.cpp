/**
 * Everyone loves value.
 *
 */

#include <JuceHeader.h>

#include "../util/Trace.h"

#include "../model/ParameterConstants.h"
#include "../model/Session.h"
#include "../model/MobiusConfig.h"
#include "../model/Preset.h"
#include "../model/Structure.h"
#include "../model/UIParameter.h"
#include "../model/UIParameterHandler.h"
#include "../model/Symbol.h"
#include "../model/ParameterProperties.h"
#include "../model/ExValue.h"
#include "../model/Enumerator.h"
#include "../model/UIAction.h"

#include "../script/MslEnvironment.h"
#include "../script/MslValue.h"
#include "../script/MslBinding.h"

#include "core/Function.h"

#include "MobiusInterface.h"

#include "MobiusShell.h"
#include "MobiusKernel.h"

#include "Valuator.h"

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

Valuator::Valuator()
{
}

Valuator::~Valuator()
{
    // return the bindings to the MslEnvironment
    for (auto track : midiTracks) {
        if (track->bindings != nullptr) {
            if (msl == nullptr) {
                Trace(1, "Valuator: Unable to return bindings during destruction");
            }
            else {
                clearBindings(track);
            }
        }
    }
}

void Valuator::initialize(SymbolTable* s, MslEnvironment* e)
{
    symbols = s;
    msl = e;
}

/**
 * This may be called multiple times during the Valuator's lifetime.
 * The objects remain stable between calls to configure.
 */
void Valuator::configure(MobiusConfig* mc, Session* s)
{
    configuration = mc;
    session = s;
    
    audioActive = session->audioTracks;
    midiActive = session->midiTracks;
    
    initTracks();
}

//////////////////////////////////////////////////////////////////////
//
// Track Management
//
//////////////////////////////////////////////////////////////////////

const int ValuatorMaxMidiTracks = 16;
//const int ValuatorMaxAudioTracks = 32;

/**
 * Valuator maintains a value binding context for each possible track.
 * Only MIDI tracks use this right now, but old Mobius audio tracks
 * will eventually.
 *
 * Here we've touching on the dynamic track problem and threads.  This code is
 * running at initialization time in the shell so it can allocadte memory, but
 * once things are running, the kernel needs to access those arrays without
 * disruption.
 *
 * One of several things that needs more thought.  For now, pre-allocate it with enough
 * for the maximum normal configuration.
 *
 */
void Valuator::initTracks()
{
    // start by only dealing with MIDI tracks
    if (midiActive > ValuatorMaxMidiTracks)
      midiActive = ValuatorMaxMidiTracks;
    
    for (int i = midiTracks.size() ; i < ValuatorMaxMidiTracks ; i++) {
        Track* t = new Track();
        t->number = audioActive + 1 + i;
        t->midi = true;
        midiTracks.add(t);
    }

    // find the Session::Track for each MIDI track
    // Session sucks right now because tracks don't have a unique number
    // like they do everywhere else, they have an "index" and a "type"
    
    for (int i = 0 ; i < midiActive ; i++) {
        Track* t = midiTracks[i];
        Session::Track* st = session->ensureTrack(Session::TypeMidi, i);
        t->session = st;
    }
}

/**
 * Locate the track data for a public track number.
 */
Valuator::Track* Valuator::getTrack(int number)
{
    Valuator::Track* found = nullptr;
    if (number > audioActive) {
        int index = number - audioActive - 1;
        if (index < ValuatorMaxMidiTracks)
          found = midiTracks[index];
    }

    // trace up here so we don't have to duplicate this in every caller
    if (found == nullptr)
      Trace(1, "Valuator: Invalid track id %d", number);
    
    return found;
}

//////////////////////////////////////////////////////////////////////
//
// Parameter Bindings
//
// !! holy shit, MslEnvironment object pools are NOT thread safe
// how did you miss that?  
//
//////////////////////////////////////////////////////////////////////

/**
 * Here when a track receives an action to change the value of
 * a parameter.  Tracks may choose to cache some parameters in local
 * class members, the rest will be maintained by Valuator.
 *
 * Parameter bindings are temporary and normally cleared on the next Reset.
 */
void Valuator::bindParameter(int trackId, UIAction* a)
{
    Track* t = getTrack(trackId);
    if (t != nullptr) {
        SymbolId symid = a->symbol->id;
        MslBinding* existing = nullptr;
        for (MslBinding* b = t->bindings ; b != nullptr ; b = b->next) {
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
                Trace(1, "Valuator: Unexpected null value in binding");
                value = msl->allocValue();
                existing->value = value;
            }
        }
        else {
            value = msl->allocValue();
            MslBinding* b = msl->allocBinding();
            b->symbolId = symid;
            b->value = value;
            b->next = t->bindings;
            t->bindings = b;
        }

        // todo: only expecting ordinals right now
        value->setInt(a->value);

        // activePreset is special, store it here so we don't have to keep
        // digging it out of the binding list
        if (symid == ParamActivePreset)
          t->activePreset = a->value;
    }
}

/**
 * Clear the temporary parameter bindings.
 * Sigh, this is called by the MidiTrack constructor which goes through its
 * Reset processing which wants to clear bindings.  Valuator will not have been
 * initialized yet.
 */
void Valuator::clearBindings(int trackId)
{
    // test for full initialization
    if (symbols != nullptr) {
        Track* t = getTrack(trackId);
        if (t != nullptr)
          clearBindings(t);
    }
}

void Valuator::clearBindings(Track* track)
{
    MslBinding* b = track->bindings;
    while (b != nullptr) {
        MslBinding* next = b->next;
        msl->free(b);
        b = next;
    }
    track->bindings = nullptr;
}

//////////////////////////////////////////////////////////////////////
//
// Group 1: MidiTracks pulling things from the Session only
//
// Used only by MIDI tracks
// Now that we've reworked how getParameterOrdinal deals with the Session
// Should just use that.
//
//////////////////////////////////////////////////////////////////////

Session::Track* Valuator::getSessionTrack(int number)
{
    Session::Track* result = nullptr;
    Track* t = getTrack(number);
    if (t != nullptr)
      result = t->session;

    if (result == nullptr)
      Trace(1, "Valuator: Missing Session::Track for %d", number);
    
    return result;
}

SyncSource Valuator::getSyncSource(int number)
{
    SyncSource result = SYNC_NONE;
    Session::Track* st = getSessionTrack(number);
    if (st != nullptr)
      result = (SyncSource)Enumerator::getOrdinal(symbols,
                                                  ParamSyncSource,
                                                  st->getParameters(),
                                                  result);
    return result;
}

SyncTrackUnit Valuator::getTrackSyncUnit(int number)
{
    SyncTrackUnit result = TRACK_UNIT_LOOP;
    Session::Track* st = getSessionTrack(number);
    if (st != nullptr)
      result = (SyncTrackUnit)Enumerator::getOrdinal(symbols,
                                                     ParamTrackSyncUnit,
                                                     st->getParameters(),
                                                     result);
    return result;
}

SyncUnit Valuator::getSlaveSyncUnit(int number)
{
    SyncUnit result = SYNC_UNIT_BEAT;
    Session::Track* st = getSessionTrack(number);
    if (st != nullptr)
      result = (SyncUnit)Enumerator::getOrdinal(symbols,
                                                ParamSlaveSyncUnit,
                                                st->getParameters(),
                                                result);
    return result;
}

LeaderType Valuator::getLeaderType(int number)
{
    LeaderType result = LeaderNone;
    Session::Track* st = getSessionTrack(number);
    if (st != nullptr) 
      result = (LeaderType)Enumerator::getOrdinal(symbols,
                                                  ParamLeaderType,
                                                  st->getParameters(),
                                                  result);
    return result;
}

LeaderLocation Valuator::getLeaderSwitchLocation(int number)
{
    LeaderLocation result = LeaderLocationNone;
    Session::Track* st = getSessionTrack(number);
    if (st != nullptr)
      result = (LeaderLocation)Enumerator::getOrdinal(symbols,
                                                      ParamLeaderSwitchLocation,
                                                      st->getParameters(),
                                                      result);
    return result;
}

int Valuator::getLoopCount(int number)
{
    int result = 2;
    Session::Track* st = getSessionTrack(number);
    if (st != nullptr) {
        Symbol* s = symbols->getSymbol(ParamLoopCount);
        if (s != nullptr) {
            MslValue* v = st->get(s->name);
            if (v != nullptr) {
                result = v->getInt();
                if (result < 1) {
                    Trace(1, "Valuator: Malformed LoopCount parameter in session %d", number);
                    result = 1;
                }
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

Preset* Valuator::getPreset(Track* track)
{
    Preset* preset = nullptr;
    
    int ordinal = track->activePreset;
    if (ordinal >= 0)
      preset = configuration->getPreset(ordinal);
    
    // fall back to the default
    // !! should be in the Session
    // actually Presets should go away entirely for MIDI tracks
    if (preset == nullptr)
      preset = configuration->getPresets();

    return preset;
}

/**
 * The primary mechanism to access parameter values from within the kernel.
 *
 * For audio tracks values come from a Preset for MIDI tracks the Session.
 */
int Valuator::getParameterOrdinal(int trackId, SymbolId symbolId)
{
    // kludge for MidiTrack that wants to call this in it's construcdtor before
    // we're initialized
    if (symbols == nullptr) return 0;
    
    Track* track = getTrack(trackId);
    Symbol* s = symbols->getSymbol(symbolId);
    int ordinal = 0;

    if (track == nullptr) {
        // already traced this
    }
    else if (s == nullptr) {
        Trace(1, "Valuator: Unmapped symbol id %d", symbolId);
    }
    else if (s->parameterProperties == nullptr) {
        Trace(1, "Valuator: Symbol %s is not a parameter", s->getName());
    }
    else if (symbolId == ParamActivePreset) {
        // this one is special
        ordinal = track->activePreset;
    }
    else {
        MslBinding* binding = nullptr;

        // first look for a binding
        for (MslBinding* b = track->bindings ; b != nullptr ; b = b->next) {
            // MSL doesn't use symbol ids, only names, but since we're overloading
            // MslBinding for use in Valuator, we will use ids by convention
            // hmm, cleaner if Valuator just had it's own object pool for this
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
            if (track->midi && track->session != nullptr) {

                ValueSet* params = track->session->getParameters();
                if (params != nullptr) {
                    MslValue* v = params->get(s->name);
                    if (v != nullptr) {
                        ordinal = v->getInt();
                        foundInSession = true;
                    }
                }
            }

            if (!foundInSession) {

                switch (s->parameterProperties->scope) {
                    case ScopeSync: {
                        // not of interest to the core
                        // new tracks get these from the Session
                    }
                        break;
                    case ScopeGlobal: {
                        // MidiTrack should be getting these from the Session
                        //Trace(1, "Valuator: Kernel attempt to access global parameter %s",
                        //s->getName());
                    }
                        break;
                    case ScopePreset: {
                        Preset* p = getPreset(track);
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
                                //Trace(1, "Valuator: Kernel attempt to access track parameter %s",
                                //s->getName());
                                }
                        break;
                    case ScopeTrack: {
                        // mostly for levels which MidiTrack should be intercepting
                        //Trace(1, "Valuator: Kernel attempt to access track parameter %s",
                        //s->getName());
                    }
                        break;
                    case ScopeUI: {
                        // not expecting this from kernel tracks
                        //Trace(1, "Valuator: Kernel attempt to access UI parameter %s",
                        //s->getName());
                    }
                        break;
                    case ScopeNone: {
                        Trace(1, "Valuator: Kernel attempt to access unscoped parameter %s",
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

ParameterMuteMode Valuator::getMuteMode(int trackId)
{
    return (ParameterMuteMode)getParameterOrdinal(trackId, ParamMuteMode);
}

SwitchLocation Valuator::getSwitchLocation(int trackId)
{
    return (SwitchLocation)getParameterOrdinal(trackId, ParamSwitchLocation);
}

SwitchDuration Valuator::getSwitchDuration(int trackId)
{
    return (SwitchDuration)getParameterOrdinal(trackId, ParamSwitchDuration);
}

SwitchQuantize Valuator::getSwitchQuantize(int trackId)
{
    return (SwitchQuantize)getParameterOrdinal(trackId, ParamSwitchQuantize);
}

QuantizeMode Valuator::getQuantizeMode(int trackId)
{
    return (QuantizeMode)getParameterOrdinal(trackId, ParamQuantize);
}

EmptyLoopAction Valuator::getEmptyLoopAction(int trackId)
{
    return (EmptyLoopAction)getParameterOrdinal(trackId, ParamEmptyLoopAction);
}

//////////////////////////////////////////////////////////////////////
//
// Sustain
//
//////////////////////////////////////////////////////////////////////

/**
 * This just tries to replicate the old functionality using ths same model,
 * which is an ugly combination of Function flags and a Preset parameter sustsinFunctions
 */
bool Valuator::isSustain(int trackId, Function* f)
{
    (void)f;
    bool sustain = false;

    if (trackId < 1) {
        Trace(1, "Valuator: Invalid track number %d", trackId);
    }
    else if (trackId <= audioActive) {
        // todo: lots of nonesense around Parameters and sustainFunctions parameter
    }
    else {
        // in the MIDI range
        // don't have Presets here so I guess sustainFunctions if we keep them
        // would be in the Session root
        // else used FunctionProperties, and should be doing this  consisntently in
        // audio tracks
    }

    return sustain;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
