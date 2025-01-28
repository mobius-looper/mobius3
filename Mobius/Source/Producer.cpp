
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
 * Handler for MainMenu/MainWindow, request a session load with an ordinal
 * that is an index into the array of names returned by getSessionNames.
 *
 * When we had Setups the MainMenu changed them by submitting a UIAction
 * using the activeSetup Symbol.  Sessions don't have a symbol yet, and I don't
 * think I want that to be the interface for changing them.
 */
Session* Producer::readSession(int ordinal)
{
    Session* session = nullptr;
    
    juce::OwnedArray<SessionClerk::Folder>* folders = clerk->getFolders();
    if (ordinal >= 0 && ordinal < folders->size()) {
        SessionClerk::Folder *f = (*folders)[ordinal];
        session = clerk->readSession(f->name);
    }
    else {
        Trace(1, "Producer: Session ordinal out of range %d", ordinal);
    }
    return session;
}

//////////////////////////////////////////////////////////////////////
//
// SessionManager Interface
//
//////////////////////////////////////////////////////////////////////

/**
 * Special interface for MainMenu/MainWindow
 * Return the list of "recent" sessions.  The index of the items in this
 * list will be passed to changeSession(ordinal)
 */
void Producer::getRecentSessions(juce::StringArray& names)
{
    getSessionNames(names);
}

/**
 * Interface for SessionManager.
 * Return the list of ALL sessions.
 */
void Producer::getSessionNames(juce::StringArray& names)
{
    juce::OwnedArray<SessionClerk::Folder>* folders = clerk->getFolders();
    for (auto folder : *folders)
      names.add(folder->name);
}

/**
 * In theory this should look into the current session and return true
 * if it has unsaved changes.
 *
 * In practice, that's really hard to do.  Just about any menu item
 * or action sent to the engine would technically modify the session.
 */
bool Producer::isSessionModified()
{
    return false;
}

Producer::Result Producer::loadSession(juce::String name)
{
    (void)name;
    return Producer::Result();
}

Producer::Result Producer::newSession(juce::String name)
{
    (void)name;
    return Producer::Result();
}

Producer::Result Producer::copySession(juce::String name, juce::String newName)
{
    (void)name;
    (void)newName;
    return Producer::Result();
}

Producer::Result Producer::renameSession(juce::String name, juce::String newName)
{
    (void)src;
    (void)newName;
    return Producer::Result();
}

Producer::Result Producer::deleteSession(juce::String name)
{
    (void)name;
    return Producer::Result();
}

bool SessionManagerTable::hasInvalidCharacters(juce::String name)
{
    return name.containsAnyOf("\\/$.");
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
