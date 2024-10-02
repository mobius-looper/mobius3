
#include <JuceHeader.h>

#include "model/ParameterConstants.h"
#include "model/Session.h"
#include "model/MobiusConfig.h"
#include "model/Preset.h"
#include "model/Structure.h"

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

SwitchLocation ParameterFinder::getSwitchLocation(MidiTracker* t, SwitchLocation dflt)
{
    SwitchLocation result = dflt;
    
    MobiusKernel* kernel = t->getKernel();
    MobiusConfig* config = kernel->getMobiusConfig();
    int activePreset = kernel->getActivePreset();
    Preset* preset = config->getPreset(activePreset);
    if (preset == nullptr)
      Trace(1, "ParameterFinder: Unable to determine Preset");
    else
      result = preset->getSwitchLocation();

    return result;
}

SwitchQuantize ParameterFinder::getSwitchQuantize(MidiTracker* t, SwitchQuantize dflt)
{
    SwitchQuantize result = dflt;
    
    MobiusKernel* kernel = t->getKernel();
    MobiusConfig* config = kernel->getMobiusConfig();
    int activePreset = kernel->getActivePreset();
    Preset* preset = config->getPreset(activePreset);
    if (preset == nullptr)
      Trace(1, "ParameterFinder: Unable to determine Preset");
    else
      result = preset->getSwitchQuantize();

    return result;
}

QuantizeMode ParameterFinder::getQuantizeMode(MidiTracker* t, QuantizeMode dflt)
{
    QuantizeMode result = dflt;
    
    MobiusKernel* kernel = t->getKernel();
    MobiusConfig* config = kernel->getMobiusConfig();
    int activePreset = kernel->getActivePreset();
    Preset* preset = config->getPreset(activePreset);
    if (preset == nullptr)
      Trace(1, "ParameterFinder: Unable to determine Preset");
    else
      result = preset->getQuantize();

    return result;
}

