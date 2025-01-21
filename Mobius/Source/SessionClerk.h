/**
 * Subcomponent of Producer that deals with Session files.
 */

#pragma once

#include <JuceHeader.h>

#include "model/Session.h"

class SessionClerk
{
  public:

    class Folder {
      public:

        /**
         * The user visible leaf folder name
         */
        juce::String name;

        /**
         * The full path to the folder
         */
        juce::String path;

        /**
         * True if the folder exists and has been verified
         */
        bool valid = false;
    };

    SessionClerk(class Provider* p);
    ~SessionClerk();

    void initialize();

    class Session* readDefaultSession();
    Session* readSession(juce::String name);
    void saveSession(Session* s);
    
  private:

    class Provider* provider = nullptr;
    juce::OwnedArray<Folder> folders;
    juce::File libraryRoot;
    bool libraryValid = false;

    Folder* findFolder(juce::String name);
    
    juce::XmlElement* readSessionElement(juce::File src);
    void logErrors(const char* filename, juce::StringArray& errors);
    
    Session* readSession(Folder* f);
    void writeSession(Folder* f, Session* s);

    Folder* bootstrapDefaultSession();

    // Temporary migration
    
    void createSession(class Session* neu);
    void migrateSetups(class MobiusConfig* src);
    void migrateSetup(class Setup* src, class Session* dest);
    void migrateTrack(class SetupTrack* src, Session::Track* dest);
    void migrateGlobals(class MobiusConfig* src, class Session* dest);
    void convertEnum(juce::String name, int value, class ValueSet* dest);
    
};
