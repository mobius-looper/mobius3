
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

// early attempt at this, probably should merge with the finder
#include "model/Enumerator.h"

#include "Provider.h"

// severe hackery
#include "mobius/midi/MidiTracker.h"
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

/**
 * MIDI tracks don't have the concepts of "default preset" and "active preset".
 * Just use the first or default preset from MobiusConfig
 */
Preset* ParameterFinder::getPreset(MidiTracker* t)
{
    MobiusKernel* kernel = t->getKernel();
    MobiusConfig* config = kernel->getMobiusConfig();
    Preset* preset = config->getPresets();
    if (preset == nullptr)
      Trace(1, "ParameterFinder: Unable to determine Preset");
    return preset;
}

SwitchLocation ParameterFinder::getSwitchLocation(MidiTracker* t, SwitchLocation dflt)
{
    SwitchLocation result = dflt;
    Preset* preset = getPreset(t);
    if (preset != nullptr)
      result = preset->getSwitchLocation();
    return result;
}

SwitchDuration ParameterFinder::getSwitchDuration(MidiTracker* t, SwitchDuration dflt)
{
    SwitchDuration result = dflt;
    Preset* preset = getPreset(t);
    if (preset != nullptr)
      result = preset->getSwitchDuration();
    return result;
}

SwitchQuantize ParameterFinder::getSwitchQuantize(MidiTracker* t, SwitchQuantize dflt)
{
    SwitchQuantize result = dflt;
    Preset* preset = getPreset(t);
    if (preset != nullptr)
      result = preset->getSwitchQuantize();
    return result;
}

QuantizeMode ParameterFinder::getQuantizeMode(MidiTracker* t, QuantizeMode dflt)
{
    QuantizeMode result = dflt;
    Preset* preset = getPreset(t);
    if (preset != nullptr)
      result = preset->getQuantize();
    return result;
}

EmptyLoopAction ParameterFinder::getEmptyLoopAction(MidiTracker* t, EmptyLoopAction dflt)
{
    EmptyLoopAction result = dflt;
    Preset* preset = getPreset(t);
    if (preset != nullptr)
      result = preset->getEmptyLoopAction();
    return result;
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
int ParameterFinder::getParameterOrdinal(MidiTracker* t, SymbolId id)
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

    return ordinal;
}

ParameterMuteMode ParameterFinder::getMuteMode(MidiTracker* t)
{
    return (ParameterMuteMode)getParameterOrdinal(t, ParamMuteMode);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
