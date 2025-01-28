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

    Producer(class Provider* p);
    ~Producer();

    void initialize();

    class Session* readStartupSession();
    Session* readSession(int ordinal);
    void saveSession(class Session* s);
    Session* changeSession(juce::String name);

    void getSessionNames(juce::StringArray& names);

    juce::String createSession(juce::String name);
    juce::String copySession(juce::String src, juce::String dest);
    juce::String renameSession(juce::String src, juce::String newName);
    juce::String deleteSession(juce::String src);

  private:

    class Provider* provider = nullptr;

    std::unique_ptr<class SessionClerk> clerk;
    
};


    
