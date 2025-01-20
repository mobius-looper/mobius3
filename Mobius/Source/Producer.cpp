
#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/Session.h"

#include "Provider.h"
#include "FileManager.h"

#include "Producer.h"

Producer::Producer(Provider* p)
{
    provider = p;
}

Producer::~Producer()
{
}

Session* Producer::readDefaultSession()
{
    Session* session = nullptr;
    
    FileManager* fm = provider->getFileManager();

    session = fm->readDefaultSession();
    if (session == nullptr) {
        // bootstrap a default session
        // shouldn't be happening unless the .xml files are missing
        session = new Session();
        session->setModified(true);
    }

    return session;
}

void Producer::writeDefaultSession(Session* s)
{
    FileManager* fm = provider->getFileManager();
    fm->writeDefaultSession(s);
    s->setModified(false);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
