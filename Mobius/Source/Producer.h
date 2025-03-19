/**
 * The Producer's job is to organize Sessions.
 * Sure, I could have called this SessionManager but that's boring.
 *
 * Mobius is always operating within an Active Session.  This is usually read from
 * the file system, but there may also be a transient unsaved operating session.
 *
 * On disk, Sessions are stored as a directory containing a session.xml file and
 * any number of associated files including audio and midi content.
 *
 * By default Session directories are organized under the user installation folder
 *    c:/Users/<username> or /Users/<username>/Library
 *
 * Eventually it can be a SystemConfig preference to point to locations outside
 * the standard installation directories.
 *
 */

#pragma once

#include <JuceHeader.h>

class Producer
{
  public:

    class Result {
      public:
        juce::StringArray errors;
    };

    Producer(class Supervisor* s);
    ~Producer();

    void initialize();

    // Supervisor Interface

    class Session* readStartupSession();
    void saveSession(class Session* s);
    Session* changeSession(juce::String name);

    // Menu Handlers
    void getRecentSessions(juce::StringArray& names);
    Session* changeSession(int ordinal);

    // SessionManager Interface
    juce::String getActiveSessionName();
    void getSessionNames(juce::StringArray& names);
    bool isSessionModified();
    Result loadSession(juce::String name);
    Result newSession(juce::String name);
    Result copySession(juce::String name, juce::String destName);
    Result renameSession(juce::String name, juce::String newName);
    Result deleteSession(juce::String name);

    // MCL Interface
    class Session* readSession(juce::String name);
    Result validateSessionName(juce::String name);
    Result writeSession(Session* s);
    
  private:

    class Supervisor* supervisor = nullptr;

    std::unique_ptr<class SessionClerk> clerk;
    
};


    
