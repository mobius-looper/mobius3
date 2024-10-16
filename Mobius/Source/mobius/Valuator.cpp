/**
 * Like the earlier ParameterFinder, this is going to start with a mess of different
 * interfaces just to start getting things funneling through Valuator.
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

#include "MobiusInterface.h"

#include "MobiusShell.h"
#include "MobiusKernel.h"

#include "Valuator.h"

Valuator::Valuator(MobiusShell* s)
{
    shell = s;
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

void Valuator::initialize(MobiusConfig* config, Session* session)
{
    (void)config;
    
    msl = shell->getContainer()->getMslEnvironment();
    symbols = shell->getContainer()->getSymbols();
    
    audioActive = session->audioTracks;
    midiActive = session->midiTracks;
    
    initTracks();

    // todo: the Session is where the concept of the "default preset"
    // will live for MIDI tracks, need to dig that out and
    // set it on each Track
}

void Valuator::reconfigure(MobiusConfig* config, Session* session)
{
    (void)config;
    (void)session;
    // work to do...
}

//////////////////////////////////////////////////////////////////////
//
// Track Management
//
//////////////////////////////////////////////////////////////////////

const int ValuatorMaxMidiTracks = 16;
const int ValuatorMaxAudioTracks = 32;

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
    for (int i = 0 ; i < ValuatorMaxMidiTracks ; i++) {
        Valuator::Track* t = new Valuator::Track();
        t->number = audioActive + i;
        midiTracks.add(t);
    }
}

/**
 * Locate the track data for a normalized track number.
 */
Valuator::Track* Valuator::getTrack(int number)
{
    Valuator::Track* found = nullptr;
    if (number >= audioActive) {
        int index = number - audioActive;
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
// Group 1: MidiTracks pulling things from the Session
//
//////////////////////////////////////////////////////////////////////

SyncSource Valuator::getSyncSource(Session::Track* trackdef, SyncSource dflt)
{
    return (SyncSource)Enumerator::getOrdinal(symbols,
                                              ParamSyncSource,
                                              trackdef->getParameters(),
                                              dflt);
}

SyncTrackUnit Valuator::getTrackSyncUnit(Session::Track* trackdef, SyncTrackUnit dflt)
{
    return (SyncTrackUnit)Enumerator::getOrdinal(symbols,
                                                 ParamTrackSyncUnit,
                                                 trackdef->getParameters(),
                                                 dflt);
}

SyncUnit Valuator::getSlaveSyncUnit(Session::Track* trackdef, SyncUnit dflt)
{
    return (SyncUnit)Enumerator::getOrdinal(symbols,
                                            ParamSlaveSyncUnit,
                                            trackdef->getParameters(),
                                            dflt);
}

int Valuator::getLoopCount(Session::Track* trackdef, int dflt)
{
    int result = dflt;
    Symbol* s = symbols->getSymbol(ParamLoopCount);
    MslValue* v = trackdef->get(s->name);
    if (v != nullptr)
      result = v->getInt();
    return result;
}

//////////////////////////////////////////////////////////////////////
//
// Group 2: Things currently in the Preset
//
//////////////////////////////////////////////////////////////////////

Preset* Valuator::getPreset(Track* track)
{
    Preset* preset = nullptr;
    
    // use the Kernel's since that's the one that will be calling this the most
    // hmm, dangerous
    MobiusConfig* config = shell->getKernel()->getMobiusConfig();
    
    int ordinal = track->activePreset;
    if (ordinal >= 0)
      preset = config->getPreset(ordinal);
    
    // fall back to the default
    // !! should be in the Session
    if (preset == nullptr)
      preset = config->getPresets();

    return preset;
}

/**
 * The primary mechanism to access parameter values from within the kernel.
 *
 * Values currently come from a Preset by default and may be overridden with
 * track-specific value bindings.
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
            // only Preset right now
            switch (s->parameterProperties->scope) {
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
