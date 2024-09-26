
#include <JuceHeader.h>

#include "model/ParameterConstants.h"
#include "model/Session.h"

// early attempt at this, probably should merge with the finder
#include "model/Enumerator.h"

#include "Provider.h"
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
                                              trackdef->parameters.get(),
                                              dflt);
}

SyncTrackUnit ParameterFinder::getTrackSyncUnit(Session::Track* trackdef, SyncTrackUnit dflt)
{
    return (SyncTrackUnit)Enumerator::getOrdinal(provider->getSymbols(),
                                                 ParamTrackSyncUnit,
                                                 trackdef->parameters.get(),
                                                 dflt);
}

SyncUnit ParameterFinder::getSlaveSyncUnit(Session::Track* trackdef, SyncUnit dflt)
{
    return (SyncUnit)Enumerator::getOrdinal(provider->getSymbols(),
                                            ParamSlaveSyncUnit,
                                            trackdef->parameters.get(),
                                            dflt);
}

