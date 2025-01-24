
#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/Session.h"
#include "model/SystemConfig.h"

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
    Session* session = nullptr;
    
    SystemConfig* sys = provider->getSystemConfig();
    juce::String name = sys->getStartupSession();

    if (name.length() > 0) {
        session = clerk->readSession(name);
        // clerk does enough trace, don't need to add more
    }

    if (session == nullptr)
      session = clerk->readDefaultSession();

    // Clerk returned a dummy Session if the library was corrupt
    return session;
}

void Producer::saveSession(Session* s)
{
    clerk->saveSession(s);
}

/**
 * This both reads the session and saves it as the startup session.
 */
Session* Producer::changeSession(juce::String name)
{
    Session* session = clerk->readSession(name);
    if (session != nullptr) {
        SystemConfig* sys = provider->getSystemConfig();
        sys->setStartupSession(name);
        provider->updateSystemConfig();
    }
    return session;
}

/**
 * Special interface for MainMenu/MainWindow
 * Return the list of "recent" sessions.  The index of the items in this
 * list will be passed to changeSession(ordinal)
 */
void Producer::getSessionNames(juce::StringArray& names)
{
    juce::OwnedArray<SessionClerk::Folder>* folders = clerk->getFolders();
    for (auto folder : *folders)
      names.add(folder->name);
}

/**
 * Handler for MainMenu/MainWindow, request a session load with an ordinal
 * that is an index into the array of names returned by getSessionNames.
 *
 * When we had Setups the MainMenu changed them by submitting a UIAction
 * using the activeSetup Symbol.  Sessions don't have a symbol yet, and I don't
 * think I want that to be the interface for changing them.
 */
void Producer::changeSession(int ordinal)
{
    Trace(1, "Producer: Change session by ordinal not implemented %d", ordinal);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
