
#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/Session.h"
#include "model/SystemConfig.h"

#include "Supervisor.h"
#include "SessionClerk.h"

#include "Producer.h"

Producer::Producer(Supervisor* s)
{
    supervisor = s;
    clerk.reset(new SessionClerk(s));
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
    juce::StringArray errors;
    
    SystemConfig* sys = supervisor->getSystemConfig();
    juce::String name = sys->getStartupSession();

    if (name.length() > 0) {
        session = clerk->readSession(name, errors);
        // clerk does enough trace, don't need to add more
    }

    if (session == nullptr)
      session = clerk->readDefaultSession(errors);

    // Clerk returned a dummy Session if the library was corrupt
    // do something about startup errors...
    
    return session;
}

void Producer::saveSession(Session* s)
{
    juce::StringArray errors;
    clerk->saveSession(s, errors);
    // what to do with errors?
}

/**
 * This both reads the session and saves it as the startup session.
 */
Session* Producer::changeSession(juce::String name)
{
    juce::StringArray errors;
    Session* session = clerk->readSession(name, errors);
    if (session != nullptr) {
        SystemConfig* sys = supervisor->getSystemConfig();
        sys->setStartupSession(name);
        supervisor->updateSystemConfig();
    }

    // we're in a menu handler so no place for errors to go unless
    // you want to pop up a window
    
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
        juce::StringArray errors;
        session = clerk->readSession(f->name, errors);
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

juce::String Producer::getActiveSessionName()
{
    Session* s = supervisor->getSession();
    return s->getName();
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
    Producer::Result result;

    Session* neu = clerk->readSession(name, result.errors);

    if (neu != nullptr) {
        supervisor->loadSession(neu);

        SystemConfig* sys = supervisor->getSystemConfig();
        sys->setStartupSession(name);
        supervisor->updateSystemConfig();
    }

    return result;
}

Producer::Result Producer::newSession(juce::String name)
{
    Producer::Result result;
    
    Session* neu = new Session();
    neu->setName(name);

    // sessiosn must have at least one looping track
    // 8 has been the default for a long time but may want to lower that
    neu->reconcileTrackCount(Session::TypeAudio, 8);

    clerk->createSession(neu, result.errors);

    delete neu;
    return result;
}

/**
 * Copy is more than just reading and writing under a differet name
 * if this session has associated content files.  Clerk must make
 * a recursive copy of the entire directory.
 *
 * Future options include copy without content, or some way to share
 * content.
 */
Producer::Result Producer::copySession(juce::String name, juce::String newName)
{
    Producer::Result result;

    clerk->copySession(name, newName, result.errors);

    return result;
}

Producer::Result Producer::renameSession(juce::String name, juce::String newName)
{
    Producer::Result result;

    clerk->renameSession(name, newName, result.errors);

    return result;
}

Producer::Result Producer::deleteSession(juce::String name)
{
    Producer::Result result;

    clerk->deleteSession(name, result.errors);

    return result;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
