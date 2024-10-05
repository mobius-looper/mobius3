
#include <JuceHeader.h>

#include "model/ParameterConstants.h"
#include "model/Session.h"
#include "model/MobiusConfig.h"
#include "model/Preset.h"
#include "model/Structure.h"
#include "model/UIParameter.h"
#include "model/UIParameterHandler.h"
#include "model/Symbol.h"
#include "model/ParameterProperties.h"
#include "model/ExValue.h"
#include "script/MslValue.h"

// early attempt at this, probably should merge with the finder
#include "model/Enumerator.h"

#include "Provider.h"

// severe hackery
#include "mobius/midi/MidiTracker.h"
#include "mobius/midi/MidiTrack.h"
#include "mobius/MobiusKernel.h"

#include "ParameterFinder.h"


ParameterFinder::ParameterFinder(Provider* p)
{
    provider = p;
}

ParameterFinder::~ParameterFinder()
{
}

SyncSource ParameterFinder::getSyncSource(Session::Track* trackdef, SyncSource dflt)
{
    return (SyncSource)Enumerator::getOrdinal(provider->getSymbols(),
                                              ParamSyncSource,
                                              trackdef->getParameters(),
                                              dflt);
}

SyncTrackUnit ParameterFinder::getTrackSyncUnit(Session::Track* trackdef, SyncTrackUnit dflt)
{
    return (SyncTrackUnit)Enumerator::getOrdinal(provider->getSymbols(),
                                                 ParamTrackSyncUnit,
                                                 trackdef->getParameters(),
                                                 dflt);
}

SyncUnit ParameterFinder::getSlaveSyncUnit(Session::Track* trackdef, SyncUnit dflt)
{
    return (SyncUnit)Enumerator::getOrdinal(provider->getSymbols(),
                                            ParamSlaveSyncUnit,
                                            trackdef->getParameters(),
                                            dflt);
}

//
// Here begings the ugly
// Anything that takes a MidiTrack as an argument is being called from the kernel.
// Just to get things fleshed out we're going with an enormous violation of encapsulation
// and reaching down into the inner classes to get the job done.
// Probably there need to be several ParameterFinders, one that just deals with the Kernel
// and another that just deals with configuration objects like Session
//

Preset* ParameterFinder::getPreset(MidiTrack* t)
{
    int ordinal = t->getActivePreset();
    MobiusKernel* kernel = t->getTracker()->getKernel();
    MobiusConfig* config = kernel->getMobiusConfig();
    if (ordinal >= 0)
      Preset* preset = config->getPreset(ordinal);
    
    if (preset == nullptr) {
        // fall back to the default
        // !! should be in the Session
        preset = config->getPresets();
    }
    
    return preset;
}

/**
 * This is for Query in MIDI tracks.
 * Here what is being requested is specified by the user.
 * In the future this will need to handle parameter set hierarchies for bindings
 * as all other parameter accessors will, but for now it falls back to the Preset
 * and Setup.
 *
 * The rest of the ones above should be re-implemented to use this.
 *
 * Since most Querys come from the InstantParameters element which is used for
 * both audio and MIDI tracks, if MidiTrack didn't intercept the query, don't emit
 * a trace error since it will happen all the time.
 */
int ParameterFinder::getParameterOrdinal(MidiTrack* t, SymbolId id)
{
    int ordinal = 0;
    
    Symbol* s = provider->getSymbols()->getSymbol(id);
    if (s == nullptr) {
        Trace(1, "ParameterFinder: Unmapped symbol id %d", id);
    }
    else if (s->parameterProperties == nullptr) {
        Trace(1, "ParameterFinder: Symbol %s is not a parameter", s->getName());
    }
    else {
        MslValue* v = t->getParameter(s->name);
        if (v != nullptr)
          ordinal = v->getInt();
        else {
            switch (s->parameterProperties->scope) {
                case ScopeGlobal: {
                    // MidiTrack should be getting these from the Session
                    //Trace(1, "ParameterFinder: Kernel attempt to access global parameter %s",
                    //s->getName());
                }
                    break;
                case ScopePreset: {
                    Preset* p = getPreset(t);
                    if (p != nullptr) {
                        ExValue value;
                        UIParameterHandler::get(id, p, &value);
                        ordinal = value.getInt();
                    }
                
                }
                    break;
                case ScopeSetup: {
                    // if 
                            // not sure why MidiTrack would want things here
                            //Trace(1, "ParameterFinder: Kernel attempt to access track parameter %s",
                            //s->getName());
                            }
                    break;
                case ScopeTrack: {
                    // mostly for levels which MidiTrack should be intercepting
                    //Trace(1, "ParameterFinder: Kernel attempt to access track parameter %s",
                    //s->getName());
                }
                    break;
                case ScopeUI: {
                    // not expecting this from kernel tracks
                    //Trace(1, "ParameterFinder: Kernel attempt to access UI parameter %s",
                    //s->getName());
                }
                    break;
                case ScopeNone: {
                    Trace(1, "ParameterFinder: Kernel attempt to access unscoped parameter %s",
                          s->getName());
                }
                    break;
            }
        }
    }
    
    return ordinal;
}

ParameterMuteMode ParameterFinder::getMuteMode(MidiTrack* t)
{
    return (ParameterMuteMode)getParameterOrdinal(t, ParamMuteMode);
}

SwitchLocation ParameterFinder::getSwitchLocation(MidiTrack* t)
{
    return (SwitchLocation)getParameterOrdinal(t, ParamSwitchLocation);
}

SwitchDuration ParameterFinder::getSwitchDuration(MidiTrack* t)
{
    return (SwitchDuration)getParameterOrdinal(t, ParamSwitchDuration);
}

SwitchQuantize ParameterFinder::getSwitchQuantize(MidiTrack* t)
{
    return (SwitchQuantize)getParameterOrdinal(t, ParamSwitchQuantize);
}

QuantizeMode ParameterFinder::getQuantizeMode(MidiTrack* t)
{
    return (QuantizeMode)getParameterOrdinal(t, ParamQuantize);
}

EmptyLoopAction ParameterFinder::getEmptyLoopAction(MidiTrack* t)
{
    return (EmptyLoopAction)getParameterOrdinal(t, ParamEmptyLoopAction);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
