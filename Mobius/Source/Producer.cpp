
#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/Session.h"

#include "Provider.h"
#include "SessionClerk.h"

#include "Producer.h"

Producer::Producer(Provider* p)
{
    provider = p;
    clerk.reset(new SessionClerk(p));
}

Producer::~Producer()
{
}

void Producer::initialize()
{
    clerk->initialize();
}

Session* Producer::readStartupSession()
{
    return clerk->readDefaultSession();
}

void Producer::saveSession(Session* s)
{
    clerk->saveSession(s);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
